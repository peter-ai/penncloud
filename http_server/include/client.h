#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "http_request.h"
#include "http_response.h"

class Client 
{
// fields
public:
    static const std::string DOUBLE_CRLF;
    HttpRequest req;
    HttpResponse res;

private:
    int m_client_fd;        // client's bound fd

// methods
public:
    // client initialized with an associated file descriptor
    Client(int client_fd) : m_client_fd(client_fd) {}
    // disable default constructor - Client should only be created with an associated fd
    Client() = delete; 

    void read_from_network();   // run server
};

#endif