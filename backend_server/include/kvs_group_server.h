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

    // checkpointing methods
    void checkpoint(std::vector<char> &inputs); // handle checkpoint operation initiated by primary
    void done(std::vector<char> &inputs);       // handle done message after checkpointing is complete

    // 2PC primary coordination methods
    void execute_two_phase_commit(std::vector<char> &inputs); // coordinates 2PC for client that requested a write operation
    int construct_and_send_prepare(uint32_t operation_seq_num, std::string &command, std::string &row, std::unordered_map<int, int> &secondary_servers);
    bool handle_secondary_votes(uint32_t operation_seq_num, std::unordered_map<int, int> &secondary_servers); // handle vote (secy/secn) from secondary
    std::vector<char> construct_and_send_commit(uint32_t operation_seq_num, std::string &command, std::string &row, std::vector<char> &inputs, std::unordered_map<int, int> &secondary_servers);
    std::vector<char> construct_and_send_abort(uint32_t operation_seq_num, std::string &row, std::unordered_map<int, int> &secondary_servers);

    // 2PC secondary response methods
    void prepare(std::vector<char> &inputs); // handle prepare msg from primary
    void commit(std::vector<char> &inputs);  // handle prepare msg from primary
    void abort(std::vector<char> &inputs);   // handle prepare msg from primary

    // Tablet write operations
    // Need to send a copy of inputs here because secondary receives exact copy of this command, and inputs is modified heavily in write operations
    std::vector<char> execute_write_operation(std::string &command, std::string &row, std::vector<char> inputs);
    std::vector<char> putv(std::string &row, std::vector<char> &inputs);
    std::vector<char> cput(std::string &row, std::vector<char> &inputs);
    std::vector<char> delr(std::string &row, std::vector<char> &inputs);
    std::vector<char> delv(std::string &row, std::vector<char> &inputs);
    std::vector<char> rnmr(std::string &row, std::vector<char> &inputs);
    std::vector<char> rnmc(std::string &row, std::vector<char> &inputs);

    // Client response methods
    void send_error_response(const std::string &msg);    // constructs an error response and internally calls send_response()
    void send_response(std::vector<char> &response_msg); // sends response to group_server_fd on open connection

    // 2PC state cleanup
    void clean_operation_state(std::unordered_map<int, int> secondary_servers); // close connections to all secondaries

    // log writing
    int write_to_log(std::string &log_filename, uint32_t operation_seq_num, const std::string &message);
    int write_to_log(std::string &log_filename, uint32_t operation_seq_num, const std::vector<char> &message);
};

#endif