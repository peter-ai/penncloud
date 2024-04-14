#ifndef KVS_CLIENT_H
#define KVS_CLIENT_H

#include <string>

class KVSClient 
{
// fields
private:
    static const char delimiter;
    static const std::string ok;
    static const std::string err;

private:
    int client_fd;        // client's bound fd

// methods
public:
    // client initialized with an associated file descriptor
    KVSClient(int client_fd) : client_fd(client_fd) {};
    // disable default constructor - Client should only be created with an associated fd
    KVSClient() = delete; 

    void read_from_network();   // read data from network (thread function)

private:
    // read first 4 bytes from stream to get command and then call corresponding command
    void handle_command(std::vector<char>& client_stream);
    void getr(std::vector<char>& inputs);
    void getv(std::vector<char>& inputs);
    void putv(std::vector<char>& inputs);
    void cput(std::vector<char>& inputs);
    void delr(std::vector<char>& inputs);
    void delv(std::vector<char>& inputs);
    void send_response(std::vector<char>& response_msg);
};

#endif