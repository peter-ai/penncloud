#ifndef KVS_GROUP_SERVER_H
#define KVS_GROUP_SERVER_H

#include <string>
#include <memory>
#include <sys/socket.h> // recv
#include <unistd.h>     // close

#include "tablet.h"
#include "backend_server.h"
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
    // read first 4 bytes from stream to get command and then call corresponding command
    void handle_command(std::vector<char> &byte_stream);

    // helpers
    std::vector<char> construct_prepare_cmd(std::string &row);

    // group communication methods
    // methods used by PRIMARY
    void initiate_two_phase_commit(std::vector<char> &inputs); // coordinates 2PC for client that requested a write operation
    void handle_secondary_vote(std::vector<char> &inputs);     // handle vote (secy/secn) from secondary
    void handle_secondary_acks(std::vector<char> &inputs);     // handle ack from secondary

    // methods used by SECONDARY
    void prepare(std::vector<char> &inputs); // handle prepare msg from primary
    void commit(std::vector<char> &inputs);  // handle prepare msg from primary
    void abort(std::vector<char> &inputs);   // handle prepare msg from primary

    // write methods
    std::vector<char> putv(std::vector<char> &inputs);
    std::vector<char> cput(std::vector<char> &inputs);
    std::vector<char> delr(std::vector<char> &inputs);
    std::vector<char> delv(std::vector<char> &inputs);

    // send response to client
    void send_response(std::vector<char> &response_msg);
};

#endif