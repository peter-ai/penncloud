/*
 * loadbalancer.cc
 *
 *  The  LB sits on top of an HTTP server is placed between the clients and the other HTTPS/frontend servers and acts as the first point of contact for all incoming network traffic from the clients.
 *  Frontend servers send a message to the load balancer upon startup (or periodically) that includes their port number. This message serves as a registration or heartbeat, indicating they are active and available.
 *
 * The LB:
 * 1. Accepts the initial HTTP request on PORT 5000 from the client
 * 2. Receives heartbeats from FrontEnd servers on PORT 4000
 * 3. Determines the best server to handle the request based on load and server availability
 * 4. Client is redirected  to the frontend server for all subsequent requests
 * 4. If there is a frontend server failure, client is redirected to the LB to be assigned a new server
 *
 */

#include "../include/loadbalancer.h"

namespace LoadBalancer
{
    using namespace std;

    std::map<int, ServerData> servers;
    std::vector<int> activeServers;
    std::map<int, std::mutex> serverMutexes;
    std::mutex server_mutex;
    int client_listen_port;
    int server_listen_port;

    // logger
    Logger loadbalancer_logger("Load Balancer");

    // set up TCP socket bound to specified port
    int create_socket(int port)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            loadbalancer_logger.log("Could not create socket", 40);
            close(sock);

            return -1;
        }
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
        {
            loadbalancer_logger.log("Setsockopt", 40);
            close(sock);
            return -1;
        }

        if (::bind(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            loadbalancer_logger.log("Could not bind to port", 40);
            close(sock);
            return -1;
        }

        if (listen(sock, 10) < 0)
        {
            loadbalancer_logger.log("Could not listen on port" + to_string(port), 40);
            close(sock);
            return -1;
        }
        return sock;
    }

    void initialize_servers(int numServers, int startingPort)
    {
        for (int i = 0; i < numServers; ++i)
        {
            int port = startingPort + i;
            loadbalancer_logger.log("Iniializing server on port " + to_string(port), 20);
            servers[port] = ServerData{std::chrono::steady_clock::now(), false}; // Mark as dead initially
            serverMutexes[port];                                                 // Create a corresponding mutex
        }
    }

    void health_check()
    {
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Periodic check every half second (double the ping frequency).
            auto now = std::chrono::steady_clock::now();                 // Mark the current time.
            std::vector<int> activeServersVec;                           // maintain a vector of active servers

            for (auto &server : servers)
            {
                serverMutexes.at(server.first).lock(); // Lock server for reading
                auto &data = server.second;
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - data.lastHeartbeat);
                if (duration.count() > 5)
                {
                    data.isActive = false; // Mark server as down if no heartbeat in last 5 seconds.
                }
                else
                {
                    data.isActive = true;                     // Server is up.
                    activeServersVec.push_back(server.first); // Add server ID to the active list.
                }
                serverMutexes.at(server.first).unlock(); // unlock server
            }

            // update vector of active servers with most recent active servers
            server_mutex.lock(); // Lock active servers to replace it with updated list
            activeServers = activeServersVec;
            server_mutex.unlock();
        }
    }

    // listen_port: frontend server will contact with its pings
    void receive_heartbeat()
    {
        int sockfd = create_socket(server_listen_port);
        if (sockfd < 0)
        {
            loadbalancer_logger.log("Failed to create or bind socket on port " + to_string(server_listen_port), 40);
            return;
        }

        loadbalancer_logger.log("Listening for heartbeats on port " + to_string(server_listen_port), 20);

        while (true)
        {
            struct sockaddr_in cliaddr;
            socklen_t len = sizeof(cliaddr);
            int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
            if (connfd < 0)
            {
                loadbalancer_logger.log("Failed to accept connection", 20);
                continue; // Continue to accept the next connection
            }

            char buffer[1024] = {0};
            ssize_t n = read(connfd, buffer, sizeof(buffer)); // Read the heartbeat message
            if (n > 0)
            {
                std::string msg(buffer, n);
                // Expected message format: "PING<SP>PORT\r\n"
                if (msg.find("PING ") == 0 && msg.rfind("\r\n") == msg.size() - 2) // check if message starts with PING and ends with CRLF
                {
                    std::string port_str = msg.substr(5, msg.size() - 7);
                    int server_port = std::stoi(port_str);
                    serverMutexes.at(server_port).lock();                                     // Lock server mutex
                    servers.at(server_port).lastHeartbeat = std::chrono::steady_clock::now(); // Update last heartbeat time and mark as active
                    servers.at(server_port).isActive = true;                                  // mark server as active in case it was previously dead
                    serverMutexes.at(server_port).unlock();                                   // Unlock server mutex
                    loadbalancer_logger.log("PING received from FE server (PORT " + port_str + ")", 20);
                }
            }
            close(connfd); // Close the connection socket
        }
    }

    std::string select_server()
    {
        server_mutex.lock(); // Lock active servers for thread safety

        // Check if no active servers exist
        if (activeServers.empty())
        {
            server_mutex.unlock();
            return "No active server available";
        }

        // Generate a random index
        std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<> dis(0, activeServers.size() - 1);
        int randomIndex = dis(generator);

        loadbalancer_logger.log("index is " + std::to_string(randomIndex) + ")", 20);

        // Retrieve the server associated with the random index
        int server_port = activeServers.at(randomIndex);
        server_mutex.unlock(); // unlock active servers

        // Return the port number of the current server as a string
        return std::to_string(server_port);
    }

    void client_handler(const HttpRequest &request, HttpResponse &response)
    {
        loadbalancer_logger.log("Entering client handler", 20); // DEBUG

        {
            // client is assigned a server that is alive
            std::string server = select_server();

            if (server == "No active server available")
            {
                loadbalancer_logger.log("No active server available", 20);
                response.set_code(503); // Set HTTP status code to Service Unavailable
                response.set_header("Content-Type", "text/html");
                response.append_body_str("503 Service Unavailable");
            }
            else
            {
                loadbalancer_logger.log("Redirecting client to server with port " + server, 20);
                // Format the server address properly assuming HTTP protocol and same host
                std::string redirectUrl = "http://localhost:" + server + "/login.html";
                // Set the response code to redirect and set the location header to the port of the frontend server it picked
                response.set_code(307);                       // HTTP 307 Temporary Redirect
                response.set_header("Location", redirectUrl); // Redirect to the server at the specified port
                response.set_header("Content-Type", "text/html");
                response.append_body_str("<html><body>Temporary Redirect to <a href='" + redirectUrl + "'>" + redirectUrl + "</a></body></html>");
            }
        }
    }

    //TO DO: change server map to ordered map on Key {8000 : ..., 8001 : ..., .... : ....}
    void lb_to_admin(int admin_port)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            loadbalancer_logger.log("Could not create socket for ADMIN console", 40);
            return;
        }
        struct sockaddr_in lb_addr;
        memset(&lb_addr, 0, sizeof(lb_addr));
        lb_addr.sin_family = AF_INET;
        lb_addr.sin_port = htons(admin_port);
        lb_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock, (struct sockaddr *)&lb_addr, sizeof(lb_addr)) < 0)
        {
            loadbalancer_logger.log("Failed to connect to ADMIN console", 40);
            close(sock);
            return;
        }

        // Expected message format: "PING<SP>NAME<SP>PORT,<SP>NAME<SP>PORT, ...,\r\n"
        std::string message = "L ";
        int count = 1;
        for (auto &server : servers)
        {
            message += "FE_Server" + to_string(count) + " " + to_string(server.first) + ", ";
            count++;
        }
        message += "\r\n";
        send(sock, message.c_str(), message.length(), 0);
        loadbalancer_logger.log("List of FE servers sent to Admin Console", 20);
        close(sock);
    }
}