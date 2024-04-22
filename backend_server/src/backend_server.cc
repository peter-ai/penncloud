#include <iostream>
#include <arpa/inet.h>

#include "../include/backend_server.h"
#include "../../utils/include/utils.h"
#include "../utils/include/be_utils.h"

// Initialize logger with info about start and end of key range
Logger be_logger = Logger("Backend server");

// initialize constant members
const int BackendServer::coord_port = 4999;
const std::unordered_set<std::string> BackendServer::commands{{"getr", "getv", "putv", "cput", "delr", "delv", "pwrt", "secy", "secn"}};

// initialize static members to default values
int BackendServer::port = 0;
std::string BackendServer::range_start = "";
std::string BackendServer::range_end = "";
int BackendServer::num_tablets = 0;
std::atomic<bool> BackendServer::is_primary(false);
int BackendServer::primary_port = 0;
std::vector<int> BackendServer::secondary_ports;
int BackendServer::server_sock_fd = -1;
int BackendServer::coord_sock_fd = -1;
std::vector<std::shared_ptr<Tablet>> BackendServer::server_tablets;
std::atomic<int> BackendServer::seq_num(0);
std::priority_queue<HoldbackOperation, std::vector<HoldbackOperation>, std::greater<HoldbackOperation>> BackendServer::holdback_operations;
std::unordered_map<int, std::unordered_set<int>> BackendServer::msg_acks_recvd;

void BackendServer::run()
{
    // // re-initialize logger to display key range
    // be_logger = Logger("Backend server " + BackendServer::range_start + ":" + BackendServer::range_end);

    // Bind storage server to its port
    if (bind_server_socket() < 0)
    {
        be_logger.log("Failed to initialize server. Exiting.", 40);
        return;
    }
    be_logger.log("Backend server bound on port " + std::to_string(BackendServer::port), 20);

    // // initialize server's state from coordinator
    // if (initialize_state_from_coordinator() < 0)
    // {
    //     be_logger.log("Failed to initialize server state from coordinator. Exiting.", 40);
    //     close(coord_sock_fd);
    //     return;
    // }
    // ! DEFAULT VALUES TO SET UP KVS (above is commented out for now)
    is_primary = true;
    primary_port = 6001;
    secondary_ports = {6000};
    range_start = "a";
    range_end = "z";
    // ! DEFAULT VALUES TO SET UP KVS

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

    initialize_tablets();         // initialize static tablets based on supplied key range
    send_coordinator_heartbeat(); // dispatch thread to send heartbeats to coordinator
    accept_and_handle_clients();  // run main server loop to accept client connections
}

int BackendServer::bind_server_socket()
{
    // create server socket
    if ((BackendServer::server_sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        be_logger.log("Unable to create server socket.", 40);
        return -1;
    }

    // bind server socket to server's port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    int opt = 1;
    if ((setsockopt(BackendServer::server_sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0)
    {
        be_logger.log("Unable to reuse port to bind server socket.", 40);
        return -1;
    }

    if ((bind(BackendServer::server_sock_fd, (const sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
        be_logger.log("Unable to bind server socket to port.", 40);
        return -1;
    }

    // listen for connections on port
    // ! check if this value is okay
    const int BACKLOG = 20;
    if ((listen(BackendServer::server_sock_fd, BACKLOG)) < 0)
    {
        be_logger.log("Unable to listen for client connections.", 40);
        return -1;
    }
    return 0;
}

int BackendServer::initialize_state_from_coordinator()
{
    // talk to coordinator to get server assignment (primary/secondary) and key range
    be_logger.log("Contacting coordinator on port " + std::to_string(BackendServer::coord_port), 20);

    // create socket for coordinator communication
    coord_sock_fd = BeUtils::open_connection(coord_port);
    if (coord_sock_fd < 0)
    {
        be_logger.log("Failed to open connection with coordinator. Exiting", 40);
        return -1;
    }

    // send initialization message to coordinator to inform coordinator that this server is starting up
    std::string msg = "INIT";
    if (BeUtils::write_to_coord(msg) < 0)
    {
        be_logger.log("Failure while sending INIT message to coordinator", 40);
        return -1;
    }

    std::string coord_response = BeUtils::read_from_coord();
    // coordinator didn't send back a full response (\r\n at the end)
    if (coord_response.empty())
    {
        be_logger.log("Failed to receive initialization details from coordinator", 40);
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

void BackendServer::initialize_tablets()
{
    // initialize tablets across servers key range

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

void ping()
{
    // Sleep for 1 seconds before sending first heartbeat
    std::this_thread::sleep_for(std::chrono::seconds(1));
    while (true)
    {
        std::string ping = "PING";
        BeUtils::write_to_coord(ping);

        // Sleep for 5 seconds before sending subsequent heartbeat
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void BackendServer::send_coordinator_heartbeat()
{
    // TODO commented out because coordinator is not alive atm (prints error messages that connection failed)
    // // create and detach thread to ping coordinator
    // be_logger.log("Sending heartbeats to coordinator", 20);
    // std::thread heartbeat_thread(ping);
    // heartbeat_thread.detach();
}

void BackendServer::accept_and_handle_clients()
{
    be_logger.log("Backend server accepting clients on port " + std::to_string(BackendServer::port), 20);
    while (true)
    {
        // accept client connection, which returns a fd for the client
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        if ((client_fd = accept(BackendServer::server_sock_fd, (sockaddr *)&client_addr, &client_addr_size)) < 0)
        {
            be_logger.log("Unable to accept incoming connection from client. Skipping.", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

        // extract port from client connection and initialize KVS_Client object
        int client_port = ntohs(client_addr.sin_port);
        KVSClient kvs_client(client_fd, client_port);
        be_logger.log("Accepted connection from client on port " + std::to_string(client_port), 20);
        be_logger.log("Client's fd is " + std::to_string(client_fd), 20);

        // launch thread to handle client
        std::thread client_thread(&KVSClient::read_from_network, &kvs_client);
        // ! fix this after everything works (manage multithreading)
        client_thread.detach();
    }
}