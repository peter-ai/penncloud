#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include <fcntl.h>
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
    static const std::string IP; // IP address

    // fields (provided at startup to run server)
    static int client_port; // port server accepts client connections on - provided at startup
    static int group_port;  // port server accepts intergroup communication on (calculated from client port)
    static int admin_port;  // port server accepts admin communication on (calculated from client port)
    static int num_tablets; // number of static tablets on this server - provided at startup

    // fields (provided by coordinator)
    static std::string range_start;                 // start of key range managed by this backend server - provided by coordinator
    static std::string range_end;                   // end of key range managed by this backend server - provided by coordinator
    static std::atomic<bool> is_primary;            // tracks if the server is a primary or secondary server (atomic since multiple client threads can read it)
    static std::atomic<int> primary_port;           // port for communication with primary - provided by coordinator
    static std::unordered_set<int> secondary_ports; // list of alive secondaries (only used by a primary to communicate with its secondaries) - provided and updated by coordinator
    static std::mutex secondary_ports_lock;         // lock for list of secondary ports, since secondary ports may be modified by thread receiving messages from coordinator

    // internal server fields
    static std::vector<std::shared_ptr<Tablet>> server_tablets; // static tablets on server (vector of shared ptrs is needed because shared_timed_mutexes are NOT copyable)
    static std::vector<std::string> tablet_ranges;              // start and end range of each tablet managed by server
    static std::string disk_dir;                                // node-local storage directory (emulates disk for a server)
    static std::atomic<bool> is_dead;                           // tracks if the server is currently dead (from an admin kill command)
    static int coord_sock_fd;                                   // fd to contact coordinator on

    // active connection fields (clients)
    static std::unordered_map<pthread_t, std::atomic<bool>> client_connections;
    static std::mutex client_connections_lock;

    // active connection fields (group servers)
    static std::unordered_map<pthread_t, std::atomic<bool>> group_server_connections;
    static std::mutex group_server_connections_lock;

    // remote-write related fields
    static uint32_t seq_num;        // write operation sequence number (used by both primary to sequence an operation, used by secondary to track operations)
    static std::mutex seq_num_lock; // lock to save sequence number for use by 2PC

    // checkpointing fields
    static std::atomic<bool> is_checkpointing; // tracks if the server is currently checkpointing (atomic since primary thread can read, but checkpointing thread can write)
    static uint32_t checkpoint_version;        // checkpoint version used by checkpointing thread to update version before initiating checkpoint procedure
    static uint32_t last_checkpoint;           // last version number of checkpoint received during checkpoint

    // methods
public:
    static void run();                                                     // run server (server does NOT run on initialization, server instance must explicitly call this method)
    static std::shared_ptr<Tablet> retrieve_data_tablet(std::string &row); // retrieve tablet containing data for thread to read from

    // public group server communication methods
    static std::unordered_map<int, int> open_connection_with_secondary_servers();                       // opens connection with each secondary. Returns list of fds for each connection.
    static void send_message_to_servers(std::vector<char> &msg, std::unordered_map<int, int> &servers); // send message to each fd in list
    static std::vector<int> wait_for_acks_from_servers(std::unordered_map<int, int> &servers);          // read from each server in map of servers. Returns vector of dead servers.

private:
    // make default constructor private
    BackendServer() {}

    // State initalization methods
    static int initialize_state_from_coordinator(int coord_sock_fd); // contact coordinator to get information
    static int initialize_tablets();                                 // initialize tablets on this server and each tablet's corresponding append-only log

    // Group communication methods
    static int dispatch_group_comm_thread();                          // dispatch thread to loop and accept communication from servers in group
    static void accept_and_handle_group_comm(int group_comm_sock_fd); // server loop to accept and handle connections from servers in its replica group

    // Coordinator communication methods
    static int dispatch_coord_comm_thread(); // dispatch thread to send heartbeat to coordinator port
    static void handle_coord_comm();         // sends heartbeat to coordinator

    // Admin communication
    static int dispatch_admin_listener_thread();                 // dispatch thread to read from admin
    static void accept_and_handle_admin_comm(int admin_sock_fd); // open connection with admin port and read messages
    static void admin_kill();                                    // handles kill command from admin console
    static void admin_live();                                    // handles live command from admin console

    // Checkpointing methods
    static void dispatch_checkpointing_thread(); // dispatch thread to checkpoint server tablets
    static void coordinate_checkpoint();         // loop for primary to initiate coordinated checkpointing

    // Client communication methods
    static void accept_and_handle_clients(); // main server loop to accept and handle clients
};

#endif