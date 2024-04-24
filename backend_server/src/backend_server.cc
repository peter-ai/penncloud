#include <iostream>
#include <arpa/inet.h>

#include "../include/backend_server.h"
#include "../../utils/include/utils.h"
#include "../utils/include/be_utils.h"
#include "../include/kvs_client.h"
#include "../include/kvs_group_server.h"

Logger be_logger = Logger("Backend server");

/**
 * BackendServer constant field initialization
 */

const int BackendServer::coord_port = 4999;

/**
 * BackendServer static field initialization
 */

int BackendServer::client_port = 0;
int BackendServer::group_port = 0;
int BackendServer::num_tablets = 0;

std::string BackendServer::range_start = "";
std::string BackendServer::range_end = "";
std::atomic<bool> BackendServer::is_primary(false);
int BackendServer::primary_port = 0;
std::vector<int> BackendServer::secondary_ports;

int BackendServer::client_comm_sock_fd = -1;
int BackendServer::group_comm_sock_fd = -1;
std::vector<std::shared_ptr<Tablet>> BackendServer::server_tablets;

int BackendServer::seq_num = 0;
std::mutex BackendServer::seq_num_lock;

std::unordered_map<uint32_t, std::vector<std::string>> BackendServer::votes_recvd;
std::mutex BackendServer::votes_recvd_lock;
std::unordered_map<uint32_t, int> BackendServer::acks_recvd;
std::mutex BackendServer::acks_recvd_lock;

/**
 * BackendServer methods
 */

void BackendServer::run()
{
    // Bind server to client connection port and store fd to accept client connections on socket
    client_comm_sock_fd = bind_socket(client_port);
    if (client_comm_sock_fd < 0)
    {
        be_logger.log("Failed to bind server to client port " + std::to_string(client_port) + ". Exiting.", 40);
        return;
    }

    // Bind server to group connection port and store fd to accept group communication on socket
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
    primary_port = 7001;
    secondary_ports = {7000};
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

    initialize_tablets();         // initialize static tablets based on supplied key range
    send_coordinator_heartbeat(); // dispatch thread to send heartbeats to coordinator
    dispatch_group_comm_thread(); // dispatch thread to loop and accept communication from servers in group
    accept_and_handle_clients();  // run main server loop to accept client connections
}

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
        secondary_ports.push_back(std::stoi(res_tokens.at(i)));
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

// @brief initialize and detach thread to listen for connections from servers in group
void BackendServer::dispatch_group_comm_thread()
{
    be_logger.log("Dispatching thread to accept and handle group communication.", 20);
    std::thread group_comm_thread(accept_and_handle_group_comm);
    group_comm_thread.detach();
}

// @brief server loop to accept and handle connections from servers in replica group
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
            be_logger.log("Unable to accept incoming connection from group server. Skipping.", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

        // extract port from group server connection and initialize KVSGroupServer object
        int group_server_port = ntohs(group_server_addr.sin_port);
        KVSGroupServer kvs_group_server(group_server_fd, group_server_port);
        be_logger.log("Accepted connection from group server on port " + std::to_string(group_server_port), 20);

        // launch thread to handle communication with group server
        std::thread group_server_thread(&KVSGroupServer::read_from_group_server, &kvs_group_server);
        // ! fix this after everything works (manage multithreading)
        group_server_thread.detach();
    }
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
            be_logger.log("Unable to accept incoming connection from client. Skipping.", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

        // extract port from client connection and initialize KVS_Client object
        int client_port = ntohs(client_addr.sin_port);
        KVSClient kvs_client(client_fd, client_port);
        be_logger.log("Accepted connection from client on port " + std::to_string(client_port), 20);

        // launch thread to handle client
        std::thread client_thread(&KVSClient::read_from_client, &kvs_client);
        // ! fix this after everything works (manage multithreading)
        client_thread.detach();
    }
}