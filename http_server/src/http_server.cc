#include "../include/http_server.h"

// *********************************************
// CONSTANTS
// *********************************************

const std::string HttpServer::version = "HTTP/1.1";
const std::unordered_set<std::string> HttpServer::supported_methods = {"GET", "HEAD", "POST"};

// *********************************************
// STATIC FIELD INITIALIZATION
// *********************************************

int HttpServer::port = -1;
int HttpServer::admin_port = -1;
std::string HttpServer::static_dir = "";
std::vector<RouteTableEntry> HttpServer::routing_table;
std::atomic<bool> HttpServer::is_dead(false);

std::unordered_map<pthread_t, std::atomic<bool>> HttpServer::client_connections;
std::mutex HttpServer::client_connections_lock;

std::shared_timed_mutex HttpServer::kvs_mutex;
std::unordered_map<std::string, std::vector<std::string>> HttpServer::client_kvs_addresses;

// http server logger
Logger http_logger("HTTP Server");

// *********************************************
// THREAD FN WRAPPER FOR SERVER CONNECTIONS
// *********************************************

void *client_thread_adapter(void *obj)
{
    Client *client = static_cast<Client *>(obj);
    client->read_from_network();
    return nullptr;
}

// *********************************************
// MAIN RUN LOGIC
// *********************************************

void HttpServer::run(int port, std::string static_dir)
{
    HttpServer::port = port;
    HttpServer::admin_port = port + 3000;
    HttpServer::static_dir = static_dir;

    // given that a load balancer is also an http server, we neeed to make sure that the load balancer is not PINGing itself!
    // check that HTTP port is not LOAD BALANCER's client_listen_port
    // excludes load balancer and admin from receiving pings and opening connection with admin console
    if (port != 7500 && port != 8082)
    {
        start_heartbeat_thread(4000, HttpServer::port); // send heartbeat to LOAD BALANCER thread that listens on port 7900

        // dispatch thread to accept communication from admin if this is not a load balancer
        if (dispatch_admin_listener_thread() < 0)
            return;
    }

    http_logger.log("HTTP server listening for connections on port " + std::to_string(HttpServer::port), 20);
    http_logger.log("Serving static files from " + HttpServer::static_dir + "/", 20);
    accept_and_handle_clients();
}

void HttpServer::run(int port)
{
    HttpServer::run(port, "static");
}

/// @brief Creates a socket and binds the server to listen on the specified port. Returns a fd if successful, -1 otherwise.
int HttpServer::bind_socket(int port)
{
    // create server socket
    int sock_fd;
    if ((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        http_logger.log("Unable to create server socket.", 40);
        return -1;
    }

    // bind server socket to provided port (ensure port can be reused)
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if ((setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0)
    {
        http_logger.log("Unable to reuse port to bind socket.", 40);
        return -1;
    }

    if ((bind(sock_fd, (const sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
        http_logger.log("Unable to bind socket to port.", 40);
        return -1;
    }

    // listen for connections on port
    const int BACKLOG = 20;
    if ((listen(sock_fd, BACKLOG)) < 0)
    {
        http_logger.log("Unable to listen for connections on bound socket.", 40);
        return -1;
    }

    http_logger.log("Server successfully bound on port " + std::to_string(port), 20);
    return sock_fd;
}

void HttpServer::accept_and_handle_clients()
{
    // Bind server to client connection port and store fd to accept client connections on socket
    int client_comm_sock_fd = bind_socket(port);
    if (client_comm_sock_fd < 0)
    {
        http_logger.log("Failed to bind server to client port " + std::to_string(port) + ". Exiting.", 40);
        return;
    }

    http_logger.log("HTTP server accepting clients on port " + std::to_string(port), 20);
    while (true)
    {
        // accept client connections as long as the server is alive
        if (!is_dead)
        {
            // join threads for clients that have been serviced
            auto it = client_connections.begin();
            for (; it != client_connections.end();)
            {
                // false indicates thread should be joined
                if (it->second == false)
                {
                    pthread_join(it->first, NULL);
                    client_connections_lock.lock();
                    it = client_connections.erase(it); // erases current value in map and re-points iterator
                    client_connections_lock.unlock();
                }
                else
                {
                    it++;
                }
            }

            // accept client connection, which returns a fd to communicate directly with the client
            int client_fd;
            struct sockaddr_in client_addr;
            socklen_t client_addr_size = sizeof(client_addr);
            if ((client_fd = accept(client_comm_sock_fd, (sockaddr *)&client_addr, &client_addr_size)) < 0)
            {
                http_logger.log("Unable to accept incoming connection from client. Skipping", 30);
                // error with incoming connection should NOT break the server loop
                continue;
            }

            if (is_dead)
            {
                close(client_fd);
                continue;
            }

            // extract port from client connection and initialize Client object
            int client_port = ntohs(client_addr.sin_port);

            // initialize Client object
            Client client(client_fd);

            http_logger.log("Accepted connection from client on port " + std::to_string(client_port), 20);
            pthread_t client_thread;
            pthread_create(&client_thread, nullptr, client_thread_adapter, &client);

            // add thread to map of client connections
            client_connections_lock.lock();
            client_connections[client_thread] = true;
            client_connections_lock.unlock();
        }
    }
}

// **************************************************
// ROUTE REGISTRATION
// **************************************************

void HttpServer::get(const std::string &path, const std::function<void(const HttpRequest &, HttpResponse &)> &route)
{
    // every GET request is also a valid HEAD request
    RouteTableEntry get_entry("GET", path, route);
    RouteTableEntry head_entry("HEAD", path, route);
    HttpServer::routing_table.push_back(get_entry);
    HttpServer::routing_table.push_back(head_entry);
    http_logger.log("Registered GET route at " + path, 20);
    http_logger.log("Registered HEAD route at " + path, 20);
}

void HttpServer::post(const std::string &path, const std::function<void(const HttpRequest &, HttpResponse &)> &route)
{
    RouteTableEntry entry("POST", path, route);
    HttpServer::routing_table.push_back(entry);
    http_logger.log("Registered POST route at " + path, 20);
}

// **************************************************
// ADMIN COMMUNICATION
// **************************************************

/// @brief initialize and detach thread to listen for messages from admin
int HttpServer::dispatch_admin_listener_thread()
{
    // Bind server to client connection port and store fd to accept client connections on socket
    int admin_sock_fd = bind_socket(admin_port);
    if (admin_sock_fd < 0)
    {
        http_logger.log("Failed to bind server to client port " + std::to_string(admin_port) + ". Exiting.", 40);
        return -1;
    }

    http_logger.log("HTTP server accepting messages from admin on port " + std::to_string(admin_port), 20);
    std::thread admin_thread(accept_and_handle_admin_comm, admin_sock_fd);
    admin_thread.detach();
    return 0;
}

/// @brief server loop to accept and handle connections from admin console
void HttpServer::accept_and_handle_admin_comm(int admin_sock_fd)
{
    while (true)
    {
        // accept connection from admin, which returns a fd to communicate directly with the server
        int admin_fd;
        struct sockaddr_in admin_addr;
        socklen_t admin_addr_size = sizeof(admin_addr);
        if ((admin_fd = accept(admin_sock_fd, (sockaddr *)&admin_addr, &admin_addr_size)) < 0)
        {
            http_logger.log("Unable to accept incoming connection from admin. Skipping", 30);
            close(admin_fd);
            // error with incoming connection should NOT break the loop
            continue;
        }

        // read from fd
        std::string result = "";
        int bytes_recvd;
        bool recvd_response = false;
        while (true)
        {
            char buf[1024]; // size of buffer for CURRENT read
            bytes_recvd = recv(admin_fd, buf, sizeof(buf), 0);

            // error while reading from source
            if (bytes_recvd < 0)
            {
                http_logger.log("Error reading from admin", 40);
                break;
            }
            // check condition where connection was preemptively closed by source
            else if (bytes_recvd == 0)
            {
                http_logger.log("Admin closed connection", 40);
                break;
            }

            for (int i = 0; i < bytes_recvd; i++)
            {
                // check last index of coordinator's response for \r and curr index in buf for \n
                if (result.length() > 0 && result.back() == '\r' && buf[i] == '\n')
                {
                    result.pop_back(); // delete \r in client message
                    recvd_response = true;
                    break;
                }
                result.push_back(buf[i]);
            }

            if (recvd_response)
            {
                break;
            }
        }

        if (result == "KILL")
        {
            admin_kill();
        }
        else if (result == "WAKE")
        {
            admin_live();
        }
        else
        {
            http_logger.log("Unrecognized command from admin. This should NOT occur", 50);
        }
        // close connection with admin after handling command
        close(admin_fd);
    }
}

/// @brief performs pseudo-kill on server
void HttpServer::admin_kill()
{
    http_logger.log("Frontend server killed by admin - shutting down", 50);
    // set flag to indicate server is dead
    is_dead = true;

    // kill all live client connection threads
    for (const auto &live_thread : client_connections)
    {
        pthread_cancel(live_thread.first);
    }

    // clear all state
    client_connections.clear(); // clear map of active client connections
}

/// @brief restarts server after pseudo kill from admin
void HttpServer::admin_live()
{
    http_logger.log("Frontend server restarted by admin", 50);
    is_dead = false;
}

// **************************************************
// KVS ADDRESS METHODS
// **************************************************

/// @brief check if the username and associated KVS server address is cached
/// @param username username associated with current session
/// @return returns true if the user is stored, false otherwise
bool HttpServer::check_kvs_addr(std::string username)
{
    HttpServer::kvs_mutex.lock_shared();
    bool present = HttpServer::client_kvs_addresses.count(username);
    HttpServer::kvs_mutex.unlock_shared();

    return present;
}

/// @brief deletes the entry associated with the given username
/// @param username username associated with current session
/// @return return true after operation is completed successfully
bool HttpServer::delete_kvs_addr(std::string username)
{
    HttpServer::kvs_mutex.lock();
    HttpServer::client_kvs_addresses.erase(username);
    HttpServer::kvs_mutex.unlock();

    return true;
}

/// @brief retrieves the KVS server address of the given user
/// @param username username associated with current session
/// @return return vector of KVS server in the form <ip,port>
std::vector<std::string> HttpServer::get_kvs_addr(std::string username)
{
    std::vector<std::string> kvs_addr;

    HttpServer::kvs_mutex.lock_shared();
    kvs_addr = HttpServer::client_kvs_addresses[username];
    HttpServer::kvs_mutex.unlock_shared();

    return kvs_addr;
}

/// @brief set the KVS address of the user
/// @param username username associated with current session
/// @param kvs_address address of the user's associated KVS server
/// @return return true after operation is completed successfully
bool HttpServer::set_kvs_addr(std::string username, std::string kvs_address)
{
    HttpServer::kvs_mutex.lock();
    HttpServer::client_kvs_addresses[username] = Utils::split(kvs_address, ":");
    HttpServer::kvs_mutex.unlock();

    return true;
}

// **************************************************
// LOAD BALANCER COMMUNICATION
// **************************************************

// send heartbeat to LOAD BALANCER
void HttpServer::send_heartbeat(int lb_port, int server_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        http_logger.log("Could not create socket for heartbeat", 40);
        return;
    }

    struct sockaddr_in lb_addr;
    memset(&lb_addr, 0, sizeof(lb_addr));
    lb_addr.sin_family = AF_INET;
    lb_addr.sin_port = htons(lb_port);
    lb_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&lb_addr, sizeof(lb_addr)) < 0)
    {
        http_logger.log("Failed to connect to load balancer for heartbeat", 40);
        close(sock);
        return;
    }

    // http_logger.log("PING sent from FE server (PORT " + std::to_string(port) + ") to Load Balancer", 20);
    // Expected message format: "PING<SP>PORT\r\n"
    std::string message = "PING " + std::to_string(server_port) + "\r\n";
    send(sock, message.c_str(), message.length(), 0);
    close(sock);
}

void HttpServer::start_heartbeat_thread(int lb_port, int server_port)
{
    {
        std::thread([=]()
                    {
        while (true) {
            if (!is_dead) {
                send_heartbeat(lb_port, server_port);
                std::this_thread::sleep_for(std::chrono::seconds(1));  // send a heartbeat every second
            }
        } })
            .detach();
    }
}
