#ifndef KVS_CLIENT_H
#define KVS_CLIENT_H

#include <poll.h>
#include <string>
#include <memory>
#include <sys/socket.h> // recv
#include <unistd.h>     // close
#include "../utils/include/be_utils.h"
#include "../include/backend_server.h"
#include "../../utils/include/utils.h"
#include "tablet.h"

class KVSClient
{
    // fields
private:
    int client_fd;   // fd to communicate with client
    int client_port; // Port that client is sending data from

    // methods
public:
    // client initialized with an associated file descriptor and client's port
    KVSClient(int client_fd, int client_port) : client_fd(client_fd), client_port(client_port){};
    // disable default constructor - Client should only be created with an associated fd and port
    KVSClient() = delete;

    void read_from_client(); // read data from client

private:
    void handle_command(std::vector<char> &client_stream);                     // read first 4 bytes from client stream and call corresponding command handler
    std::vector<char> geta();                                                  // get all rows from all tablets on server
    std::vector<char> getr(std::vector<char> &inputs);                         // get row from tablet
    std::vector<char> getv(std::vector<char> &inputs);                         // get value from tablet
    std::vector<char> forward_operation_to_primary(std::vector<char> &inputs); // forward non-read operations received from client to primary
    void send_response(std::vector<char> &response_msg);                       // send response to client
};

#endif