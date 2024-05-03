#include <iostream>
#include <arpa/inet.h>

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
int BackendServer::num_tablets = 0;

// fields provided by coordinator
std::string BackendServer::range_start = "";
std::string BackendServer::range_end = "";
std::atomic<bool> BackendServer::is_primary(false);
std::atomic<int> BackendServer::primary_port(0);
std::unordered_set<int> BackendServer::secondary_ports;
std::mutex BackendServer::secondary_ports_lock;

// server fields
int BackendServer::client_comm_sock_fd = -1;
int BackendServer::group_comm_sock_fd = -1;
std::vector<std::shared_ptr<Tablet>> BackendServer::server_tablets;

// remote-write related fields
uint32_t BackendServer::seq_num = 0;
std::mutex BackendServer::seq_num_lock;

// checkpointing fields
std::atomic<bool> BackendServer::is_checkpointing(false);
uint32_t BackendServer::checkpoint_version = 0;
uint32_t BackendServer::last_checkpoint = 0;

// *********************************************
// MAIN RUN LOGIC
// *********************************************

void BackendServer::run()
{
    // Bind server to client connection port and store fd to accept client connections on socket
    client_comm_sock_fd = bind_socket(client_port);
    if (client_comm_sock_fd < 0)
    {
        be_logger.log("Failed to bind server to client port " + std::to_string(client_port) + ". Exiting.", 40);
        return;
    }

    // Bind server to group communication port and store fd to accept group communication on socket
    group_comm_sock_fd = bind_socket(group_port);
    if (group_comm_sock_fd < 0)
    {
        be_logger.log("Failed to bind server to group port " + std::to_string(group_port) + ". Exiting.", 40);
        return;
    }

    // ! COORDINATOR STATE INITIALIZATION
    // // initialize server's state from coordinator
    // if (initialize_state_from_coordinator() < 0)
    // {
    //     be_logger.log("Failed to initialize server state from coordinator. Exiting.", 40);
    //     close(coord_sock_fd);
    //     return;
    // }
    // ! DEFAULT VALUES UNTIL COORDINATOR COMMUNICATION IS SET UP
    is_primary = true;
    primary_port = 7501;
    secondary_ports = {7500};
    range_start = "a";
    range_end = "z";

    // TODO note that all of this logging will be moved inside initialize_state_from_coordinator()
    is_primary
        ? be_logger.log("Server type [PRIMARY]", 20)
        : be_logger.log("Server type [SECONDARY]", 20);
    be_logger.log("Managing key range " + BackendServer::range_start + ":" + BackendServer::range_end, 20);

    be_logger.log("Group primary at " + std::to_string(primary_port), 20);
    std::string secondaries;
    for (int secondary_port : secondary_ports)
    {
        secondaries += std::to_string(secondary_port) + " ";
    }
    be_logger.log("Group secondaries at " + secondaries, 20);
    // ! DEFAULT VALUES UNTIL COORDINATOR COMMUNICATION IS SET UP

    initialize_tablets();         // initialize static tablets using key range from coordinator
    dispatch_group_comm_thread(); // dispatch thread to loop and accept communication from servers in group
    send_coordinator_heartbeat(); // dispatch thread to send heartbeats to coordinator
    accept_and_handle_clients();  // run main server loop to accept client connections
}

// @brief main server loop to accept and handle clients
void BackendServer::accept_and_handle_clients()
{
    be_logger.log("Backend server accepting clients on port " + std::to_string(client_port), 20);
    while (true)
    {
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

        // launch thread to handle client
        std::thread client_thread(&KVSClient::read_from_client, KVSClient(client_fd, client_port));
        // ! fix this after everything works (manage multithreading)
        client_thread.detach();
    }
}

// *********************************************
// KVS SERVER INITIALIZATION
// *********************************************

// @brief Creates a socket and binds to the specified port
int BackendServer::bind_socket(int port)
{
    // create server socket
    int sock_fd;
    if ((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        be_logger.log("Unable to create server socket.", 40);
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
        be_logger.log("Unable to reuse port to bind socket.", 40);
        return -1;
    }

    if ((bind(sock_fd, (const sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
        be_logger.log("Unable to bind socket to port.", 40);
        return -1;
    }

    // listen for connections on port
    const int BACKLOG = 20;
    if ((listen(sock_fd, BACKLOG)) < 0)
    {
        be_logger.log("Unable to listen for connections on bound socket.", 40);
        return -1;
    }

    be_logger.log("Backend server successfully bound on port " + std::to_string(port), 20);
    return sock_fd;
}

// @brief Contacts coordinator to retrieve initialization state
// ! look into this later once coordinator is complete
int BackendServer::initialize_state_from_coordinator()
{
    be_logger.log("Contacting coordinator on port " + std::to_string(BackendServer::coord_port), 20);

    // open connection with coordinator
    int coord_sock_fd = BeUtils::open_connection(coord_port);
    if (coord_sock_fd < 0)
    {
        be_logger.log("Failed to open connection with coordinator. Exiting", 40);
        return -1;
    }

    // send initialization message to coordinator to inform coordinator that this server is starting up
    std::string msg = "INIT " + std::to_string(client_port);
    if (BeUtils::write_to_coord(coord_sock_fd, msg) < 0)
    {
        be_logger.log("Failure while sending INIT message to coordinator", 40);
        return -1;
    }

    std::string coord_response = BeUtils::read_from_coord(coord_sock_fd);
    // coordinator didn't send back a full response (\r\n at the end)
    if (coord_response.empty())
    {
        be_logger.log("Failed to receive initialization state from coordinator", 40);
        return -1;
    }

    // split response into tokens and extract data
    // Assume message is of the form "P/S<SP>START_RANGE:END_RANGE<SP>PRIMARY_ADDRESS<SP>SECONDARY1\r\n"
    std::vector<std::string> res_tokens = Utils::split(coord_response, " ");

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
    primary_port = std::stoi(res_tokens.at(2));
    be_logger.log("PRIMARY AT 127.0.0.1:" + std::to_string(primary_port), 20);
    for (size_t i = 3; i < res_tokens.size(); i++)
    {
        secondary_ports.insert(std::stoi(res_tokens.at(i)));
        be_logger.log("SECONDARY AT 127.0.0.1:" + res_tokens.at(i), 20);
    }

    return 0;
}

// @brief Initialize tablets across key range provided by coordinator
void BackendServer::initialize_tablets()
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

    // Output metadata about each created tablet
    for (auto &tablet : server_tablets)
    {
        be_logger.log("Initialized tablet for range " + tablet->range_start + ":" + tablet->range_end, 20);
    }
}

// **************************************************
// COORDINATOR COMMUNICATION
// **************************************************

// ! this is NOT ready yet
void ping(int fd)
{
    // Sleep for 1 seconds before sending first heartbeat
    std::this_thread::sleep_for(std::chrono::seconds(1));
    while (true)
    {
        std::string ping = "PING";
        BeUtils::write_to_coord(fd, ping);

        // Sleep for 5 seconds before sending subsequent heartbeat
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ! this is NOT ready yet
void BackendServer::send_coordinator_heartbeat()
{
    // TODO commented out because coordinator is not alive atm (prints error messages that connection failed)
    // // create and detach thread to ping coordinator
    // be_logger.log("Sending heartbeats to coordinator", 20);
    // std::thread heartbeat_thread(ping, BackendServer::server_sock_fd);
    // heartbeat_thread.detach();
}

// **************************************************
// INTER-GROUP COMMUNICATION
// **************************************************

/// @brief initialize and detach thread to listen for connections from servers in group
void BackendServer::dispatch_group_comm_thread()
{
    be_logger.log("Dispatching thread to accept and handle group communication", 20);
    std::thread group_comm_thread(accept_and_handle_group_comm);
    group_comm_thread.detach();
}

/// @brief server loop to accept and handle connections from servers in replica group
void BackendServer::accept_and_handle_group_comm()
{
    be_logger.log("Backend server accepting messages from group on port " + std::to_string(group_port), 20);
    while (true)
    {
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

        // launch thread to handle communication with group server
        std::thread group_server_thread(&KVSGroupServer::read_from_group_server, KVSGroupServer(group_server_fd, group_server_port));
        // ! fix this after everything works (manage multithreading)
        group_server_thread.detach();
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
        BeUtils::write(server.second, msg);
    }
}

/// @brief Read from each server in map of servers. Returns vector of dead servers.
std::vector<int> BackendServer::wait_for_events_from_servers(std::unordered_map<int, int> &servers)
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
            bool is_dead = secondary_ports.count(server.first) == 0 && server.first != primary_port;
            secondary_ports_lock.unlock();

            // track server if it's dead and stop trying to read from it
            if (is_dead)
            {
                dead_servers.push_back(server.first);
                break;
            }
            BeUtils::wait_for_events({server.second}, 250);
        }
    }
    return dead_servers;
}

// **************************************************
// CHECKPOINTING
// **************************************************

// @brief initialize and detach thread to checkpoint server tablets
void BackendServer::dispatch_checkpointing_thread()
{
    // dispatch a thread to
    be_logger.log("Dispatching thread for checkpointing", 20);
    std::thread checkpointing_thread(coordinate_checkpoint);
    checkpointing_thread.detach();
}

// @brief initialize and detach thread to checkpoint server tablets
void BackendServer::coordinate_checkpoint()
{
    while (true)
    {
        // Only primary can initiate checkpointing - other servers loop here until they become primary servers (possible if primary fails)
        if (is_primary)
        {
            // Sleep for 60 seconds between each checkpoint
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
            std::vector<int> dead_servers = wait_for_events_from_servers(servers);
            // remove dead servers from map of servers
            for (int dead_server : dead_servers)
            {
                close(servers[dead_server]); // close fd for dead server
                servers.erase(dead_server);  // remove dead server from map
            }
            // read acks from all remaining servers, since these servers have read events available on their fds
            for (const auto &server : servers)
            {
                BeUtils::read(server.second); // we don't need to do anything with the ACKs
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

// @brief Retrieve tablet containing data for thread to read from
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