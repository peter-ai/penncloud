#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include <sys/socket.h> // socket
#include <netinet/in.h> // sockaddr_in
#include <thread>       // std::thread
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>

#include "tablet.h"
#include "kvs_client.h"
#include "../../utils/include/utils.h"

class BackendServer
{
    // fields
public:
    // constants
    static const std::unordered_set<std::string> supported_commands; // GET, PUT
    static const int coord_port;                                     // coordinator's port

    // backend server fields
    static int port; // port server runs on
    // NOTE: if the key range is "a" to "d", this server will manage every key UP TO AND INCLUDING "d"'s full key range
    // For example, a key called dzzzzz would be managed in this server. The next server would start at "e"
    static std::string range_start; // start of key range managed by this backend server
    static std::string range_end;   // end of key range managed by this backend server
    static int num_tablets;         // number of static tablets on this server

    // group metadata
    static bool is_primary;                  // tracks if the server is a primary or secondary server
    static int primary_port;                 // primary port (only useful is this server is a secondary)
    static std::vector<int> secondary_ports; // list of secondaries (only useful if this server is a primary)

private:
    static int server_sock_fd; // bound server socket's fd
    static int coord_sock_fd;  // fd for communication with coordinator

    // note that a vector of shared ptrs is needed because shared_timed_mutexes are NOT copyable
    static std::vector<std::shared_ptr<Tablet>> server_tablets; // static tablets on server

    friend class KVSClient;
    // methods
public:
    static void run(); // run server (server does NOT run on initialization, server instance must explicitly call this method)

private:
    // make default constructor private
    BackendServer() {}

    static int bind_server_socket();                   // bind port to socket
    static int open_connection_with_coordinator();     // open connection with coordinator
    static int write_to_coordinator(std::string &msg); // write msg to coordinator
    static std::string read_from_coordinator();        // read msg from coordinator
    static int initialize_state_from_coordinator();    // contact coordinator to get information
    static void initialize_tablets();                  // initialize tablets on this server
    static void send_coordinator_heartbeat();          // dispatch thread to send heartbeat to coordinator port
    static void accept_and_handle_clients();           // main server loop to accept and handle clients
};

#endif