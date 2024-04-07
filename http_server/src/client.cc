#include <iostream>
#include <sys/socket.h>   // recv
#include <unistd.h>       // close

#include "client.h"
#include "http_request.h"
#include "http_server.h"
#include "utils.h"

const std::string Client::CRLF = "\r\n";
const std::string Client::DOUBLE_CRLF = "\r\n\r\n";

void Client::read_from_network()
{
    std::string client_stream;
    int bytes_recvd;

    while (true) {
        char buf[4096];
        // ! error check recv function
        bytes_recvd = recv(m_client_fd, buf, 4096, 0);

        for (int i = 0 ; i < bytes_recvd ; i++) {
            client_stream += buf[i];
            // double CRLF indicates a full http request WITHOUT the body (if present)
            if (client_stream.length() >= 4 && client_stream.substr(client_stream.length() - 4) == DOUBLE_CRLF) {
                // build http request from client stream and then clear current stream
                handle_req(client_stream);
                client_stream.clear();
            }
        }
    }
    close(m_client_fd);
}


void Client::handle_req(std::string& client_stream) {
    std::vector<std::string> msg_lines = Utils::split(client_stream, CRLF);
    parse_req_line(msg_lines.at(0));
    msg_lines.erase(msg_lines.begin());
    parse_headers(msg_lines);
    validate_request();

    // check if the request has a body
    // if so, set the remaining body length value for the client

    auto content_len_it = m_req.headers.find("content_length");
    if (content_len_it != m_req.headers.end()) {
        if ((content_len_it->second).size() > 1) {
            construct_error_response(400);
        } else if ((content_len_it->second).size() == 1) {
            try {
                remaining_body_len = std::stoi((content_len_it->second).at(0));
            } catch (const std::exception& e) {
                construct_error_response(400);
            }
        }
    }

    // handle session related headers too i think



    // check if the client requested to close the connection
    // if so, set close_connection to true
    auto connection_it = m_req.headers.find("connection");
    if (connection_it != m_req.headers.end()) {
        if ((connection_it->second).size() > 1) {
            construct_error_response(400);
        } else if ((connection_it->second).size() == 1 && (connection_it->second).at(0) == "close") {
            close_connection = true;
        }
    }
}


void Client::parse_req_line(std::string& req_line)
{
    // verify req_line has 3 values
    if (req_line.size() != 3) {
        construct_error_response(400);
    }
    m_req.req_method = req_line.at(0);
    m_req.path = req_line.at(1);
    m_req.version = req_line.at(2);
}


void Client::parse_headers(std::vector<std::string> headers)
{
    for (std::string header : headers) {
        // split header
        std::vector<std::string> header_components = Utils::split_on_first_delim(header, ":");
        if (header_components.size() != 2) {
            construct_error_response(400);
        }
        std::string header_key = Utils::to_lowercase(header_components.at(0));
        std::string header_values = Utils::trim(header_components.at(1));

        // ! store headers in m_req
    }
}


void Client::construct_error_response(int err_code)
{
    // response is ready to send back to client
    response_ready = true;
}


// void Client::validate_request()
// {
//     // host headers not present in parsed headers


//     // unsupported http version
//     if (m_req.version != HttpServer::version) {

//     }

//     // unsupported method
//     if (m_req.version != HttpServer::version) {

//     }
// }