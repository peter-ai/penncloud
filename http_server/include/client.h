#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <iostream>
#include <sys/socket.h> // recv
#include <unistd.h>     // close
#include <fstream>
#include <sstream>

#include "http_response.h"
#include "http_request.h"
#include "http_server.h"
#include "../../utils/include/utils.h"

class Client
{
    // fields
public:
    static const std::string CRLF;
    static const std::string DOUBLE_CRLF;
    // note that requests/responses for a client are sequential, so after a response is sent, both req and res must be cleared
    HttpRequest req;     // current request
    HttpResponse res;    // response for current request
    bool response_ready; // tracks if a response was sent
    int remaining_body_len;
    bool close_connection;

private:
    int client_fd; // client's bound fd

    // methods
public:
    // client initialized with an associated file descriptor
    Client(int client_fd) : response_ready(false), remaining_body_len(0),
                            close_connection(false), client_fd(client_fd) {}
    // disable default constructor - Client should only be created with an associated fd
    Client() = delete;

    void read_from_network(); // run server
    void send_response();

private:
    void parse_req(std::string &client_stream);
    void parse_req_line(std::string &req_line);
    void parse_headers(std::vector<std::string> &headers);
    void handle_req();
    void set_req_type();
    void construct_error_response(int err_code);
    void construct_response();
};

#endif