#include <iostream>
#include <sys/socket.h>   // socket
#include <netinet/in.h>   // sockaddr_in
#include <thread>         // std::thread

#include "../include/backend_server.h"
// #include "../include/kvs_client.h"
#include "../../utils/include/utils.h"

// Initialize logger with info about start and end of key range
Logger be_logger = Logger("Backend server");

// initialize constant members
const std::unordered_set<std::string> BackendServer::supported_commands = {"GET", "PUT"};

// initialize static members to default values
int BackendServer::coord_port = 0;
int BackendServer::port = 0;
std::string BackendServer::range_start = "";
std::string BackendServer::range_end = "";
int BackendServer::num_tablets = 0;
int BackendServer::server_sock_fd = -1;                                  
std::vector<std::shared_ptr<Tablet>> BackendServer::server_tablets;      

void BackendServer::run()
{
    // re-initialize logger to display key range
    be_logger = Logger("Backend server " + BackendServer::range_start + ":" + BackendServer::range_end);
    if (bind_server_socket() < 0) {
        be_logger.log("Failed to initialize server. Exiting.", 40);
        return;
    }
    be_logger.log("Backend server listening for connections on port " + std::to_string(BackendServer::port), 20);
    be_logger.log("Managing key range " + BackendServer::range_start + ":" + BackendServer::range_end, 20);
    initialize_tablets();
    send_coordinator_heartbeat();
    // accept_and_handle_clients();
}


int BackendServer::bind_server_socket()
{
    // create server socket
    if ((BackendServer::server_sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        be_logger.log("Unable to create server socket.", 40);
        return -1;
    }

    // bind server socket to server's port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    int opt = 1;
    if ((setsockopt(BackendServer::server_sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0) {
        be_logger.log("Unable to reuse port to bind server socket.", 40);
        return -1;
    }

    if ((bind(BackendServer::server_sock_fd, (const sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
        be_logger.log("Unable to bind server socket to port.", 40);
        return -1;
    }

    // listen for connections on port
    // ! check if this value is okay
    const int BACKLOG = 20;
    if ((listen(BackendServer::server_sock_fd, BACKLOG)) < 0) {
        be_logger.log("Unable to listen for client connections.", 40);
        return -1;
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
    for (int i = 1; i <= num_tablets; i++) {
        int curr_tablet_size = chars_per_tablet;

        // Distribute the remainder chars evenly across partitions
        if (i <= remainder) {
            curr_tablet_size++;
        }

        // initialize tablet and add to server tablets
        char tablet_start = curr_char;
        char tablet_end = curr_char + curr_tablet_size - 1;
        server_tablets.push_back(std::make_shared<Tablet>(std::string(1, tablet_start), std::string(1, tablet_end)));
        curr_char += curr_tablet_size;
    }

    // Output metadata about each created tablet
    for (auto& tablet : server_tablets) {
        be_logger.log("Initialized tablet for range " + tablet->range_start + ":" + tablet->range_end, 20);
    }
}


void BackendServer::send_coordinator_heartbeat() 
{
    // create thread and send message to coordinator port
}


void BackendServer::accept_and_handle_clients()
{
    while (true) {
        // accept client connection, which returns a fd for the client
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        if ((client_fd = accept(BackendServer::server_sock_fd, (sockaddr*) &client_addr, &client_addr_size)) < 0) {
            be_logger.log("Unable to accept incoming connection from client. Skipping.", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

//         KVSClient kvs_client(client_fd);
//         be_logger.log("Accepted connection from client __________", 20);

//         // launch thread to handle client
//         std::thread client_thread(&KVSClient::read_from_network, &kvs_client);
//         // ! fix this after everything works (manage multithreading)
//         client_thread.detach();
//     }
// }