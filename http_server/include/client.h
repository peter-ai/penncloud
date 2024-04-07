#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "http_request.h"
#include "http_response.h"

class Client 
{
// fields
public:
    static const std::string CRLF;
    static const std::string DOUBLE_CRLF;
    // note that requests/responses for a client are sequential, so after a response is sent, both req and res must be cleared
    HttpRequest m_req;    // current request 
    HttpResponse m_res;   // response for current request
    bool response_ready;   // tracks if a response was sent
    bool remaining_body_len;
    bool close_connection = false;

private:
    int m_client_fd;        // client's bound fd

// methods
public:
    // client initialized with an associated file descriptor
    Client(int client_fd) : m_client_fd(client_fd) {}
    // disable default constructor - Client should only be created with an associated fd
    Client() = delete; 

    void read_from_network();   // run server

private:
    void handle_req(std::string& client_stream);
    void parse_req_line(std::string& req_line);
    void parse_headers(std::vector<std::string> headers);
    void validate_request();
    void construct_error_response(int err_code);
};

#endif