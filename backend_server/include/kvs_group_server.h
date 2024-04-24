#ifndef KVS_GROUP_SERVER_H
#define KVS_GROUP_SERVER_H

#include <string>

#include "../../utils/include/utils.h"

class KVSGroupServer
{
    // fields
private:
    int group_server_fd;   // fd to communicate with group server
    int group_server_port; // Port that group server is sending data from
    Logger kvs_group_server_logger;

    // methods
public:
    // group server initialized with an associated file descriptor and group server's port
    KVSGroupServer(int group_server_fd, int group_server_port) : group_server_fd(group_server_fd), group_server_port(group_server_port),
                                                                 kvs_group_server_logger("KVS Group Server [" + std::to_string(group_server_port) + "]"){};
    // disable default constructor - KVSGroupServer should only be created with an associated fd and port
    KVSGroupServer() = delete;

    void read_from_group_server(); // read data from group server

private:
    // read first 4 bytes from stream to get command and then handle command accordingly
    void handle_command(std::vector<char> &byte_stream);

    /**
     * GROUP COMMUNICATION METHODS
     */

    // methods used by PRIMARY
    void execute_two_phase_commit(std::vector<char> &inputs); // coordinates 2PC for client that requested a write operation
    std::vector<int> open_connection_with_secondary_fds();    // opens connection with each secondary port. Returns list of fds for each connection.
    int construct_and_send_prepare_cmd(int seq_num, std::vector<char> &inputs, std::vector<int> secondary_fds);
    void handle_secondary_vote(std::vector<char> &inputs);         // handle vote (secy/secn) from secondary
    std::string extract_row_from_input(std::vector<char> &inputs); // extract row from input operation
    void handle_secondary_ack(std::vector<char> &inputs);          // handle ack from secondary

    // methods used by SECONDARY
    void prepare(std::vector<char> &inputs); // handle prepare msg from primary
    void commit(std::vector<char> &inputs);  // handle prepare msg from primary
    void abort(std::vector<char> &inputs);   // handle prepare msg from primary

    /**
     * WRITE METHODS
     */

    // Need to send a copy of inputs here because secondary receives exact copy of this command, and inputs is modified heavily in write operations
    std::vector<char> execute_write_operation(std::vector<char> inputs);
    std::vector<char> putv(std::vector<char> &inputs);
    std::vector<char> cput(std::vector<char> &inputs);
    std::vector<char> delr(std::vector<char> &inputs);
    std::vector<char> delv(std::vector<char> &inputs);

    /**
     * CLIENT RESPONSE METHODS
     */

    void send_error_response(const std::string &msg);    // constructs an error response and internally calls send_response()
    void send_response(std::vector<char> &response_msg); // sends response to group_server_fd on open connection

    /**
     * STATE CLEANUP METHODS
     */
    void clean_operation_state(int operation_seq_num, std::vector<int> secondary_fds); // clean operation seq num from maps, close connections to secondaries
};

#endif