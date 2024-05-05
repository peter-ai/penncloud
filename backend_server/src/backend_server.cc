#include <iostream>
#include <arpa/inet.h>
#include <fstream>

#include "../include/backend_server.h"
#include "../../utils/include/utils.h"
#include "../utils/include/be_utils.h"
#include "../include/kvs_client.h"
#include "../include/kvs_group_server.h"

Logger be_logger = Logger("Backend server");

// *********************************************
// CONSTANTS
// *********************************************

const int BackendServer::coord_port = 4999;

// *********************************************
// STATIC FIELD INITIALIZATION
// *********************************************

// fields provided at startup
int BackendServer::client_port = 0;
int BackendServer::group_port = 0;
int BackendServer::admin_port = 0;
int BackendServer::num_tablets = 0;

// fields provided by coordinator
std::string BackendServer::range_start = "";
std::string BackendServer::range_end = "";
std::atomic<bool> BackendServer::is_primary(false);
std::atomic<int> BackendServer::primary_port(0);
std::unordered_set<int> BackendServer::secondary_ports;
std::mutex BackendServer::secondary_ports_lock;

// internal server fields
std::vector<std::shared_ptr<Tablet>> BackendServer::server_tablets;
std::string BackendServer::disk_dir;
std::atomic<bool> BackendServer::is_dead(false);

// active connection fields (clients)
std::unordered_map<pthread_t, std::atomic<bool>> BackendServer::client_connections;
std::mutex BackendServer::client_connections_lock;

// active connection fields (group servers)
std::unordered_map<pthread_t, std::atomic<bool>> BackendServer::group_server_connections;
std::mutex BackendServer::group_server_connections_lock;

// remote-write related fields
uint32_t BackendServer::seq_num = 0;
std::mutex BackendServer::seq_num_lock;

// checkpointing fields
std::atomic<bool> BackendServer::is_checkpointing(false);
uint32_t BackendServer::checkpoint_version = 0;
uint32_t BackendServer::last_checkpoint = 0;

// *********************************************
// THREAD FN WRAPPER FOR SERVER CONNECTIONS
// *********************************************

void *client_thread_adapter(void *obj)
{
    KVSClient *kvs_client = static_cast<KVSClient *>(obj);
    kvs_client->read_from_client();
    return nullptr;
}

void *group_server_thread_adapter(void *obj)
{
    KVSGroupServer *kvs_group_server = static_cast<KVSGroupServer *>(obj);
    kvs_group_server->read_from_group_server();
    return nullptr;
}

// *********************************************
// MAIN RUN LOGIC
// *********************************************

/// @brief Initialize server state from coordinator, dispatch threads to read from group and clients, and dispatch checkpointing thread
void BackendServer::run()
{
    /**
     * Steps
     * 1. Dispatch thread to communicate with coordinator (initializes state, creates tablets, sends heartbeats)
     * 2. Dispatch admin listener thread
     * 3. Dispatch group communication thread
     * 4. Dispatch checkpointing thread
     * 5. Accept and handle client connections
     */

    // store node local storage directory
    disk_dir = "KVS_" + std::to_string(client_port) + "/";

    // dispatch thread to communicate with coordinator
    if (dispatch_coord_comm_thread() < 0)
        return;

    // dispatch thread to accept communication from admin
    if (dispatch_admin_listener_thread() < 0)
        return;

    // dispatch thread to accept communication from servers in group
    if (dispatch_group_comm_thread() < 0)
        return;

    dispatch_checkpointing_thread(); // dispatch thread to checkpoint storage tablets
    accept_and_handle_clients();     // run main server loop to accept client connections
}

/// @brief main server loop to accept and handle clients
void BackendServer::accept_and_handle_clients()
{
    // Bind server to client connection port and store fd to accept client connections on socket
    int client_comm_sock_fd = BeUtils::bind_socket(client_port);
    if (client_comm_sock_fd < 0)
    {
        be_logger.log("Failed to bind server to client port " + std::to_string(client_port) + ". Exiting.", 40);
        return;
    }

    be_logger.log("Backend server accepting clients on port " + std::to_string(client_port), 20);
    // accept client connections as long as the server is alive
    while (!is_dead)
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
            be_logger.log("Unable to accept incoming connection from client. Skipping", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

        // extract port from client connection and initialize KVS_Client object
        int client_port = ntohs(client_addr.sin_port);
        be_logger.log("Accepted connection from client on port " + std::to_string(client_port), 20);

        // initialize KVSGroupServer object
        KVSClient kvs_client(client_fd, client_port);
        pthread_t client_thread;
        pthread_create(&client_thread, nullptr, client_thread_adapter, &kvs_client);
        pthread_create(&client_thread, nullptr, client_thread_adapter, &kvs_client);

        // add thread to map of client connections
        client_connections_lock.lock();
        client_connections[client_thread] = true;
        client_connections_lock.unlock();
    }
}

// **************************************************
// COORDINATOR COMMUNICATION
// **************************************************

/// @brief initialize and detach thread to communicate with coordinator
int BackendServer::dispatch_coord_comm_thread()
{
    // open long-running connection with coordinator
    int coord_sock_fd = BeUtils::open_connection(coord_port);
    if (coord_sock_fd < 0)
    {
        be_logger.log("Failed to open connection with coordinator. Exiting", 40);
        return -1;
    }

    be_logger.log("Backend server opened connection with coordinator on port " + std::to_string(coord_port), 20);

    // initialize server state from coordinator
    if (initialize_state_from_coordinator(coord_sock_fd) < 0)
    {
        be_logger.log("Failed to initialize server state from coordinator. Exiting", 40);
        return -1;
    }

    // initialize static tablets and create corresponding append-only log using key range from coordinator
    if (initialize_tablets() < 0)
    {
        be_logger.log("Failed to initialize server tablets. Exiting", 40);
        return -1;
    }

    // create and detach thread for subsequent communication with coordinator
    std::thread coord_comm_thread(handle_coord_comm, coord_sock_fd);
    coord_comm_thread.detach();
    return 0;
}

/// @brief sends heartbeat to coordinator at frequency of 1 second. Poll coordinator in between heartbeats.
void BackendServer::handle_coord_comm(int coord_sock_fd)
{
    be_logger.log("Sending heartbeats to coordinator", 20);
    // Sleep for 1 seconds before sending first heartbeat
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // send heartbeats as long as the server is alive
    while (!is_dead)
    {
        std::string ping = "PING";
        BeUtils::write_with_crlf(coord_sock_fd, ping);
        // Sleep for 1 seconds before sending subsequent heartbeat
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// *********************************************
// KVS SERVER INITIALIZATION
// *********************************************

/// @brief Contacts coordinator to retrieve initialization state
int BackendServer::initialize_state_from_coordinator(int coord_sock_fd)
{
    // send initialization message to coordinator to inform coordinator that this server is starting up
    std::string ip = "127.0.0.1:";
    std::string msg = "INIT " + ip + std::to_string(BackendServer::group_port);
    if (BeUtils::write_with_crlf(coord_sock_fd, msg) < 0)
    {
        be_logger.log("Failure while sending INIT message to coordinator", 40);
        return -1;
    }

    BeUtils::ReadResult coord_response = BeUtils::read_with_crlf(coord_sock_fd);
    // error in coordinator's response
    if (coord_response.error_code < 0)
    {
        be_logger.log("Failed to receive initialization state from coordinator", 40);
        return -1;
    }

    // split response into tokens and extract data
    // Assume message is of the form "P/S<SP>START_RANGE:END_RANGE<SP>PRIMARY_ADDRESS<SP>SECONDARY1\r\n"
    std::vector<std::string> res_tokens = Utils::split(std::string(coord_response.byte_stream.begin(), coord_response.byte_stream.end()), " ");

    // basic check, tokens should be at least 4 tokens long (assumes at least 1 secondary)
    if (res_tokens.size() < 4)
    {
        be_logger.log("Malformed coordinator response", 40);
        return -1;
    }

    // set if server is primary or secondary
    if (res_tokens.at(0) == "P")
    {
        is_primary = true;
    }
    else if (res_tokens.at(0) == "S")
    {
        is_primary = false;
    }
    // coordinator sent back something that it shouldn't have sent, shut down server
    else
    {
        be_logger.log("Malformed responsibility token from coordinator", 40);
        return -1;
    }

    // set start and end range
    std::vector<std::string> range = Utils::split(res_tokens.at(1), ":");
    // start and end of key range should have exactly two tokens
    if (range.size() != 2)
    {
        be_logger.log("Malformed key range", 40);
        return -1;
    }
    range_start = range.at(0);
    range_end = range.at(1);

    // save primary and list of secondaries
    primary_port = std::stoi(res_tokens.at(2).substr(ip.length()));
    std::string secondaries;
    for (size_t i = 3; i < res_tokens.size(); i++)
    {
        secondary_ports.insert(std::stoi(res_tokens.at(i).substr(ip.length())));
        secondary_ports.insert(std::stoi(res_tokens.at(i).substr(ip.length())));
        secondaries += res_tokens.at(i) + " ";
    }

    // log information about server
    is_primary
        ? be_logger.log("Server type [PRIMARY]", 20)
        : be_logger.log("Server type [SECONDARY]", 20);
    be_logger.log("Managing key range " + BackendServer::range_start + ":" + BackendServer::range_end, 20);

    be_logger.log("Group primary at " + std::to_string(primary_port), 20);
    be_logger.log("Group secondaries at " + secondaries, 20);
    return 0;
}

/// @brief Initialize tablets across key range provided by coordinator
int BackendServer::initialize_tablets()
{
    // Convert start and end characters to integers and calculate size of range
    char start = range_start[0];
    char end = range_end[0];
    int range_size = end - start + 1;

    // Calculate the number of characters per tablet
    int chars_per_tablet = range_size / num_tablets;
    int remainder = range_size % num_tablets;

    int curr_char = start;
    // loop through number of tablets and set the range for each tablet
    for (int i = 1; i <= num_tablets; i++)
    {
        int curr_tablet_size = chars_per_tablet;

        // Distribute the remainder chars evenly across partitions
        if (i <= remainder)
        {
            curr_tablet_size++;
        }

        // initialize tablet and add to server tablets
        char tablet_start = curr_char;
        char tablet_end = curr_char + curr_tablet_size - 1;
        server_tablets.push_back(std::make_shared<Tablet>(std::string(1, tablet_start), std::string(1, tablet_end)));
        curr_char += curr_tablet_size;
    }

    // Output metadata about each created tablet and create its corresponding append-only log
    for (auto &tablet : server_tablets)
    {
        // log tablet metadata
        be_logger.log("Initialized tablet for range " + tablet->range_start + ":" + tablet->range_end, 20);

        // create log file for tablet
        std::string tablet_log = disk_dir + tablet->log_filename;
        std::ofstream log_file(tablet_log);
        if (!log_file.is_open())
        {
            return -1;
        }
        log_file.close();
        be_logger.log("Created log file for tablet " + tablet->range_start + ":" + tablet->range_end, 20);
    }
    return 0;
}

// **************************************************
// INTER-GROUP COMMUNICATION
// **************************************************

/// @brief initialize and detach thread to listen for connections from servers in group
int BackendServer::dispatch_group_comm_thread()
{
    // Bind server to group communication port and store fd to accept group communication on socket
    int group_comm_sock_fd = BeUtils::bind_socket(group_port);
    if (group_comm_sock_fd < 0)
    {
        be_logger.log("Failed to bind server to group port " + std::to_string(group_port) + ". Exiting.", 40);
        return -1;
    }

    be_logger.log("Backend server accepting messages from group on port " + std::to_string(group_port), 20);
    std::thread group_comm_thread(accept_and_handle_group_comm, group_comm_sock_fd);
    group_comm_thread.detach();
    return 0;
}

/// @brief server loop to accept and handle connections from servers in replica group
void BackendServer::accept_and_handle_group_comm(int group_comm_sock_fd)
{
    // accept group connections as long as the server is alive
    while (!is_dead)
    {
        // join threads for group server connections that have been serviced
        auto it = group_server_connections.begin();
        for (; it != group_server_connections.end();)
        {
            // false indicates thread should be joined
            if (it->second == false)
            {
                pthread_join(it->first, NULL);
                group_server_connections_lock.lock();
                it = group_server_connections.erase(it); // erases current value in map and re-points iterator
                group_server_connections_lock.unlock();
            }
            else
            {
                it++;
            }
        }

        // accept connection from server in group, which returns a fd to communicate directly with the server
        int group_server_fd;
        struct sockaddr_in group_server_addr;
        socklen_t group_server_addr_size = sizeof(group_server_addr);
        if ((group_server_fd = accept(group_comm_sock_fd, (sockaddr *)&group_server_addr, &group_server_addr_size)) < 0)
        {
            be_logger.log("Unable to accept incoming connection from group server. Skipping", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

        // extract port from group server connection and initialize KVSGroupServer object
        int group_server_port = ntohs(group_server_addr.sin_port);
        be_logger.log("Accepted connection from group server on port " + std::to_string(group_server_port), 20);

        // initialize KVSGroupServer object
        KVSGroupServer kvs_group_server(group_server_fd, group_server_port);
        pthread_t group_server_thread;
        pthread_create(&group_server_thread, nullptr, &group_server_thread_adapter, &kvs_group_server);
        pthread_create(&group_server_thread, nullptr, &group_server_thread_adapter, &kvs_group_server);

        // add thread to map of group server connections connections
        group_server_connections_lock.lock();
        group_server_connections[group_server_thread] = true;
        group_server_connections_lock.unlock();
    }
}

/// @brief Open connection with each secondary port and save fd for each connection
std::unordered_map<int, int> BackendServer::open_connection_with_secondary_servers()
{
    std::unordered_map<int, int> secondary_fds;
    secondary_ports_lock.lock(); // lock secondary ports map to prevent modifications while opening connections with live servers
    for (int secondary_port : BackendServer::secondary_ports)
    {
        // retry up to 3 times to open connection with secondary port
        for (int i = 0; i < 3; i++)
        {
            int secondary_fd = BeUtils::open_connection(secondary_port);
            // successfully opened connection - insert into map and break retry loop to go to next port
            if (secondary_fd > 0)
            {
                secondary_fds[secondary_port] = secondary_fd;
                break;
            }
        }
    }
    secondary_ports_lock.unlock();
    return secondary_fds;
}

/// @brief Send message to each server in list of servers
void BackendServer::send_message_to_servers(std::vector<char> &msg, std::unordered_map<int, int> &servers)
{
    for (const auto &server : servers)
    {
        // write message to server
        // retries are integrated into write, so if write fails, it's likely due to an issue with the server
        // if an issue occurred with the server, we'll catch it when trying to read from the fd
        BeUtils::write_with_size(server.second, msg);
    }
}

/// @brief Read from each server in map of servers. Returns vector of dead servers.
std::vector<int> BackendServer::wait_for_acks_from_servers(std::unordered_map<int, int> &servers)
{
    // tracks servers that potentially died, so we can remove them from the server map. Otherwise, we must keep trying to read from the server.
    std::vector<int> dead_servers;
    // read from each server
    for (const auto &server : servers)
    {
        // if this server is alive, we must wait for it to send an ack - wait 250 ms before checking if it's dead
        while (true)
        {
            // check if server is dead
            secondary_ports_lock.lock();
            bool curr_server_is_dead = secondary_ports.count(server.first) == 0 && server.first != primary_port;
            secondary_ports_lock.unlock();

            // track server if it's dead and stop trying to read from it
            if (curr_server_is_dead)
            {
                dead_servers.push_back(server.first);
                break;
            }
            // read event is available on this server, move to waiting for ack on next server
            if (BeUtils::wait_for_events({server.second}, 250) == 0)
            {
                break;
            }
        }
    }
    return dead_servers;
}

// **************************************************
// ADMIN COMMUNICATION
// **************************************************

/// @brief initialize and detach thread to listen for messages from admin
int BackendServer::dispatch_admin_listener_thread()
{
    // Bind server to admin port and store fd to accept admin communication on socket
    int admin_sock_fd = BeUtils::bind_socket(admin_port);
    if (admin_sock_fd < 0)
    {
        be_logger.log("Failed to bind server to admin port " + std::to_string(admin_port) + ". Exiting.", 40);
        return -1;
    }

    be_logger.log("Backend server accepting messages from admin on port " + std::to_string(admin_port), 20);
    std::thread admin_thread(accept_and_handle_admin_comm, admin_sock_fd);
    admin_thread.detach();
    return 0;
}

/// @brief server loop to accept and handle connections from admin console
void BackendServer::accept_and_handle_admin_comm(int admin_sock_fd)
{
    while (true)
    {
        // accept connection from admin, which returns a fd to communicate directly with the server
        int admin_fd;
        struct sockaddr_in admin_addr;
        socklen_t admin_addr_size = sizeof(admin_addr);
        if ((admin_fd = accept(admin_sock_fd, (sockaddr *)&admin_addr, &admin_addr_size)) < 0)
        {
            be_logger.log("Unable to accept incoming connection from admin. Skipping", 30);
            close(admin_fd);
            // error with incoming connection should NOT break the loop
            continue;
        }

        // read message from admin console
        BeUtils::ReadResult admin_msg = BeUtils::read_with_crlf(admin_fd);
        if (admin_msg.error_code < 0)
        {
            be_logger.log("Error reading message from admin.", 30);
            close(admin_fd);
            continue;
        }
        // convert admin message to string, since it's string compatible
        std::string admin_msg_str(admin_msg.byte_stream.begin(), admin_msg.byte_stream.end());
        if (admin_msg_str == "KILL")
        {
            admin_kill();
        }
        else if (admin_msg_str == "LIVE")
        {
            admin_live();
        }
        else
        {
            be_logger.log("Unrecognized command from admin. This should NOT occur", 50);
        }
        // close connection with admin after handling command
        close(admin_fd);
    }
}

/// @brief performs pseudo-kill on server
void BackendServer::admin_kill()
{
    // set flag to indicate server is dead
    is_dead = true;

    // kill all live client connection threads
    for (const auto &live_thread : client_connections)
    {
        pthread_cancel(live_thread.first);
    }

    // kill all live group server connection threads
    for (const auto &live_thread : group_server_connections)
    {
        pthread_cancel(live_thread.first);
    }

    // clear all state
    server_tablets.clear();           // remove tablets from memory
    client_connections.clear();       // clear map of active client connections
    group_server_connections.clear(); // clear map of active group server connections
}

/// @brief restarts server after pseudo kill from admin
// TODO implement this function
void BackendServer::admin_live()
{
    // send RECO to coordinator
    // wait for message from coordinator about server state

    // send message to primary asking for its latest version number
    // if version number is the same, ask primary for log and perform operations. When you're done, turn off recovery mode
    // if version number is different, ask primary for its serialized tablets + its logs

    // set flag to indicate server is now alive
    // do this LAST
    is_dead = false;
}

// **************************************************
// CHECKPOINTING
// **************************************************

/// @brief initialize and detach thread to checkpoint server tablets
void BackendServer::dispatch_checkpointing_thread()
{
    // dispatch a thread to start checkpointing
    be_logger.log("Dispatching thread for checkpointing", 20);
    std::thread checkpointing_thread(coordinate_checkpoint);
    checkpointing_thread.detach();
}

/// @brief initialize and detach thread to checkpoint server tablets
void BackendServer::coordinate_checkpoint()
{
    // initiate checkpoint as long as the server is alive
    while (!is_dead)
    {
        // Only primary can initiate checkpointing - other servers loop here until they become primary servers (possible if primary fails)
        if (is_primary)
        {
            // Sleep for 30 seconds between each checkpoint
            std::this_thread::sleep_for(std::chrono::seconds(60));

            // Begin checkpointing
            checkpoint_version++; // increment checkpoint version number
            // prepare version number as vector for message sending
            std::vector<uint8_t> version_num_vec = BeUtils::host_num_to_network_vector(checkpoint_version);
            is_checkpointing = true; // Set flag to true to reject write requests
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Primary initiating checkpointing", 20);

            // open connection with all servers
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Opening connection with all servers", 20);
            std::unordered_map<int, int> servers = open_connection_with_secondary_servers();
            // open connection with primary server - retry up to 3 times
            for (int i = 0; i < 3; i++)
            {
                int primary_fd = BeUtils::open_connection(primary_port);
                // successfully opened connection with primary - break retry loop
                if (primary_fd > 0)
                {
                    servers[primary_port] = primary_fd;
                    break;
                }
            }
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Successfully opened connection with all servers", 20);

            // Send CHECKPOINT to all servers with checkpoint number appended to message
            std::vector<char> checkpoint_msg = {'C', 'K', 'P', 'T', ' '};
            checkpoint_msg.insert(checkpoint_msg.end(), version_num_vec.begin(), version_num_vec.end());
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Sending CHECKPOINT to servers", 20);
            send_message_to_servers(checkpoint_msg, servers);

            // Wait for ACKs from all live servers
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Waiting for ACKs from servers", 20);
            std::vector<int> dead_servers = wait_for_acks_from_servers(servers);

            // remove dead servers from map of servers
            for (int dead_server : dead_servers)
            {
                close(servers[dead_server]); // close fd for dead server
                servers.erase(dead_server);  // remove dead server from map
            }
            // read acks from all remaining servers, since these servers have read events available on their fds
            for (const auto &server : servers)
            {
                BeUtils::read_with_size(server.second); // we don't need to do anything with the ACKs
            }
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Received ACKs from servers", 20);

            // Send DONE to all servers with checkpoint number appended to message
            std::vector<char> done_msg = {'D', 'O', 'N', 'E', ' '};
            done_msg.insert(done_msg.end(), version_num_vec.begin(), version_num_vec.end());
            be_logger.log("CP[" + std::to_string(checkpoint_version) + "] Sending DONE to servers", 20);
            send_message_to_servers(done_msg, servers);

            // Set checkpointing flag to false to begin accepting write requests again
            is_checkpointing = false;
        }
    }
}

// *********************************************
// TABLET INTERACTION
// *********************************************

/// @brief Retrieve tablet containing data for thread to read from
std::shared_ptr<Tablet> BackendServer::retrieve_data_tablet(std::string &row)
{
    // iterate tablets in reverse order and find first tablet that row is "greater" than
    for (int i = num_tablets - 1; i >= 0; i--)
    {
        std::string tablet_start = server_tablets.at(i)->range_start;
        if (row >= tablet_start)
        {
            return server_tablets.at(i);
        }
    }
    // this should never execute
    be_logger.log("Could not find tablet for given row - this should NOT occur", 50);
    return nullptr;
}