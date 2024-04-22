#ifndef KVS_CLIENT_H
#define KVS_CLIENT_H

#include <string>
#include <memory>
#include <sys/socket.h> // recv
#include <unistd.h>     // close

#include "tablet.h"
#include "backend_server.h"
#include "../../utils/include/utils.h"

class KVSClient
{
    // fields
private:
    int client_fd;   // client's bound fd
    int client_port; // client's port

    // methods
public:
    // client initialized with an associated file descriptor and client's port
    KVSClient(int client_fd, int client_port) : client_fd(client_fd), client_port(client_port){};
    // disable default constructor - Client should only be created with an associated fd
    KVSClient() = delete;

    void read_from_network(); // read data from network (thread function)

private:
    // read first 4 bytes from stream to get command and then call corresponding command
    void handle_command(std::vector<char> &client_stream);

    // remote-writes related methods
    // Note that inputs is NOT passed by reference because a copy of inputs is needed
    // a copy is necessary because the inputs passed in will be used by the primary to perform the command after confirmation that all secondaries are done
    // if we take a reference and modify it, it'll modify the data that primary will use to perform the command after
    int send_operation_to_secondaries(std::vector<char> inputs);
    int wait_for_secondary_acks(std::vector<int> &secondary_fds); // loops and waits for secondaries to send acknowledgements
    std::vector<char> forward_operation_to_primary(std::vector<char> &inputs);

    // retrieve data tablet to service command
    std::shared_ptr<Tablet> retrieve_data_tablet(std::string &row);

    // send response to client
    void send_response(std::vector<char> &response_msg);

    // read only methods
    void getr(std::vector<char> &inputs);
    void getv(std::vector<char> &inputs);

    // write methods
    void call_write_command(std::string command, std::vector<char> &inputs);
    void putv(std::vector<char> &inputs);
    void cput(std::vector<char> &inputs);
    void delr(std::vector<char> &inputs);
    void delv(std::vector<char> &inputs);
};

#endif