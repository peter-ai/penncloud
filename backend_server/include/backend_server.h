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

struct HoldbackOperation
{
    uint32_t seq_num;
    std::vector<char> msg;

    // Constructor
    HoldbackOperation(int seq_num, std::vector<char> &msg) : seq_num(seq_num), msg(msg) {}

    // comparison operator for HoldbackOperations
    bool operator>(const HoldbackOperation &other) const
    {
        return seq_num > other.seq_num;
    }
};

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
    static std::string range_start;          // start of key range managed by this backend server - provided by coordinator
    static std::string range_end;            // end of key range managed by this backend server - provided by coordinator
    static std::atomic<bool> is_primary;     // tracks if the server is a primary or secondary server (atomic since multiple client threads can read it)
    static int primary_port;                 // port for communication with primary - provided by coordinator
    static std::vector<int> secondary_ports; // list of secondaries (only used by a primary to communicate with its secondaries) - provided by coordinator

    // server fields
    static int client_comm_sock_fd;                             // bound server socket's fd
    static int group_comm_sock_fd;                              // bound server socket's fd
    static std::vector<std::shared_ptr<Tablet>> server_tablets; // static tablets on server (vector of shared ptrs is needed because shared_timed_mutexes are NOT copyable)

    // remote-write related fields
    static std::atomic<int> seq_num;      // write operation sequence number (used by both primary and secondary to determine next operation to perform)
    static std::atomic<int> prep_seq_num; // write operation sequence number (used by both primary and secondary to determine next operation to perform)

    // holdback queue (min heap - orders operations by sequence number so the lowest sequence number is at the top of the heap)
    static std::priority_queue<HoldbackOperation, std::vector<HoldbackOperation>, std::greater<HoldbackOperation>> holdback_operations;
    static std::unordered_map<uint32_t, std::vector<std::string>> votes_recvd; // map of msg seq num to set of secondaries that have sent a SECY for that operation
    // ! we could potentially change this to row level locking but might not be worth it
    static std::mutex votes_recvd_mutex;                                         // map of msg seq num to condition variable that wakes up waiting primary
    static std::unordered_map<uint32_t, std::condition_variable> votes_recvd_cv; // map of msg seq num to condition variable that wakes up waiting primary
    static std::unordered_map<uint32_t, int> acks_recvd;                         // map of msg seq num to set of secondaries that have sent a SECY for that operation
    // ! we could potentially change this to row level locking but might not be worth it
    static std::mutex acks_recvd_mutex;                                         // map of msg seq num to condition variable that wakes up waiting primary
    static std::unordered_map<uint32_t, std::condition_variable> acks_recvd_cv; // map of msg seq num to condition variable that wakes up waiting primary

    // methods
public:
    static void run();                                                     // run server (server does NOT run on initialization, server instance must explicitly call this method)
    static std::shared_ptr<Tablet> retrieve_data_tablet(std::string &row); // retrieve tablet containing data for thread to read from

private:
    // make default constructor private
    BackendServer() {}

    static int bind_socket(int port);               // creates a socket and binds to the specified port. Returns the fd that the socket is bound to.
    static int initialize_state_from_coordinator(); // contact coordinator to get information
    static void initialize_tablets();               // initialize tablets on this server
    static void send_coordinator_heartbeat();       // dispatch thread to send heartbeat to coordinator port
    static void dispatch_group_comm_thread();       // dispatch thread to loop and accept communication from servers in group
    static void accept_and_handle_group_comm();     // server loop to accept and handle connections from servers in its replica group
    static void accept_and_handle_clients();        // main server loop to accept and handle clients
};

#endif