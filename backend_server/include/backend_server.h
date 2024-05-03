#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include <sys/socket.h> // socket
#include <netinet/in.h> // sockaddr_in
#include <thread>       // std::thread
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>
#include <queue>

#include "tablet.h"
#include "kvs_client.h"
#include "../../utils/include/utils.h"

class BackendServer
{
    // fields
public:
    // constants
    static const int coord_port; // coordinator's port

    // fields (provided at startup to run server)
    static int client_port; // port server accepts client connections on - provided at startup
    static int group_port;  // port server accepts intergroup communication on - provided at startup
    static int num_tablets; // number of static tablets on this server - provided at startup

    // fields (provided by coordinator)
    static std::string range_start;                 // start of key range managed by this backend server - provided by coordinator
    static std::string range_end;                   // end of key range managed by this backend server - provided by coordinator
    static std::atomic<bool> is_primary;            // tracks if the server is a primary or secondary server (atomic since multiple client threads can read it)
    static std::atomic<int> primary_port;           // port for communication with primary - provided by coordinator
    static std::unordered_set<int> secondary_ports; // list of secondaries (only used by a primary to communicate with its secondaries) - provided by coordinator
    static std::mutex secondary_ports_lock;         // lock for list of secondary ports, since secondary ports may be modified by thread receiving messages from coordinator

    // server fields
    static int client_comm_sock_fd;                             // bound server socket's fd for client communication
    static int group_comm_sock_fd;                              // bound server socket's fd for group communication
    static std::vector<std::shared_ptr<Tablet>> server_tablets; // static tablets on server (vector of shared ptrs is needed because shared_timed_mutexes are NOT copyable)

    // remote-write related fields
    static uint32_t seq_num;        // write operation sequence number (used by both primary and secondary to determine next operation to perform)
    static std::mutex seq_num_lock; // lock to save sequence number for use by 2PC

    // checkpointing fields
    static std::atomic<bool> is_checkpointing; // tracks if the server is currently checkpointing (atomic since multiple threads can read)
    static uint32_t checkpoint_version;        // checkpoint version (ONLY USED BY PRIMARY)
    static uint32_t last_checkpoint;           // version number of checkpoint received from primary during checkpoint

    // methods
public:
    static void run();                                                     // run server (server does NOT run on initialization, server instance must explicitly call this method)
    static std::shared_ptr<Tablet> retrieve_data_tablet(std::string &row); // retrieve tablet containing data for thread to read from

    // public group server communication methods
    static std::unordered_map<int, int> open_connection_with_secondary_servers();                       // opens connection with each secondary. Returns list of fds for each connection.
    static void send_message_to_servers(std::vector<char> &msg, std::unordered_map<int, int> &servers); // send message to each fd in list
    static std::vector<int> wait_for_events_from_servers(std::unordered_map<int, int> &servers);        // read from each server in map of servers. Returns vector of dead servers.

private:
    // make default constructor private
    BackendServer() {}

    static void accept_and_handle_clients(); // main server loop to accept and handle clients

    // KVS server state initalization
    static int bind_socket(int port);               // creates a socket and binds to the specified port. Returns the fd that the socket is bound to.
    static int initialize_state_from_coordinator(); // contact coordinator to get information
    static void initialize_tablets();               // initialize tablets on this server

    // Communication methods
    static void dispatch_group_comm_thread();   // dispatch thread to loop and accept communication from servers in group
    static void accept_and_handle_group_comm(); // server loop to accept and handle connections from servers in its replica group
    static void send_coordinator_heartbeat();   // dispatch thread to send heartbeat to coordinator port

    // Checkpointing methods
    static void dispatch_checkpointing_thread(); // dispatch thread to checkpoint server tablets
    static void coordinate_checkpoint();         // loop for primary to initiate coordinated checkpointing
};

#endif