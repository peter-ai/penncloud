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
    int client_fd; // client's bound fd

    // methods
public:
    // client initialized with an associated file descriptor
    KVSClient(int client_fd) : client_fd(client_fd){};
    // disable default constructor - Client should only be created with an associated fd
    KVSClient() = delete;

    void read_from_network(); // read data from network (thread function)

private:
    // read first 4 bytes from stream to get command and then call corresponding command
    void handle_command(std::vector<char> &client_stream);
    std::shared_ptr<Tablet> retrieve_data_tablet(std::string &row);

    // read only methods
    void getr(std::vector<char> &inputs);
    void getv(std::vector<char> &inputs);

    // remote-writes related methods
    void forward_to_primary(std::vector<char> &inputs);
    int send_operation_to_secondaries(std::vector<char> &inputs);
    int wait_for_secondary_acks(); // loops and waits for secondaries to send acknowledgements
    void putv(std::vector<char> &inputs);
    void cput(std::vector<char> &inputs);
    void delr(std::vector<char> &inputs);
    void delv(std::vector<char> &inputs);
    void send_response(std::vector<char> &response_msg);
};

#endif