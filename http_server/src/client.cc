#include <iostream>
#include <sys/socket.h>   // recv
#include <unistd.h>       // close
#include <fstream>
#include <sstream>

#include "../include/http_server.h"
#include "../include/client.h"
#include "../include/http_request.h"
#include "../../utils/include/utils.h"

const std::string Client::CRLF = "\r\n";
const std::string Client::DOUBLE_CRLF = "\r\n\r\n";

void Client::read_from_network()
{
    std::cout << "Reading data from client" << std::endl;

    std::string client_stream;
    int bytes_recvd;
    while (true) {
        char buf[4096];
        // ! error check recv function
        bytes_recvd = recv(client_fd, buf, 4096, 0);

        // error while reading from client
        if (bytes_recvd < 0) {
            // // Utils::error("Unable to receive message from client.");
            break;
        }
        // client likely closed connection
        else if (bytes_recvd == 0) {
            break;
        }

        std::cout << "Successfully read data from client" << std::endl;
        for (int i = 0 ; i < bytes_recvd ; i++) {
            // byte is part of request body
            if (remaining_body_len > 0) {
                req.body.push_back(buf[i]);
                remaining_body_len--;

                // prev req is complete - prepare and send response
                if (remaining_body_len == 0) {
                    construct_response();
                    send_response();
                }
            } 
            // byte is part of client_stream (building request)            
            else {
                client_stream += buf[i];
                // double CRLF indicates a full http request WITHOUT the body (if present)
                if (client_stream.length() >= 4 && client_stream.substr(client_stream.length() - 4) == DOUBLE_CRLF) {
                    // build http request from client stream and then clear current stream
                    handle_req(client_stream);
                    client_stream.clear();

                    // error occurred while parsing
                    if (response_ready) {
                        send_response();
                        continue;
                    }

                    // request had content-length of 0
                    if (remaining_body_len == 0) {
                        construct_response();
                        send_response();
                    }
                }
            }
        }
    }
    close(client_fd);
}


void Client::handle_req(std::string& client_stream) {
    std::cout << client_stream << std::endl;

    std::vector<std::string> msg_lines = Utils::split(client_stream, CRLF);
    parse_req_line(msg_lines.at(0));
    msg_lines.erase(msg_lines.begin());
    parse_headers(msg_lines);

    // check if request is static or dynamic
    set_request_type();

    // check if http message has a body
    if (req.headers.count("content-length") != 0) {
        std::vector<std::string>& content_len_values = req.headers["content-length"];
        // multiple content length values are stored
        if (content_len_values.size() > 1) {
            construct_error_response(400);
            return;
        } else if (content_len_values.size() == 1) {
            // content length value is not a valid integer
            try {
                remaining_body_len = std::stoi(content_len_values.at(0));
            } catch (const std::exception& e) {
                construct_error_response(400);
                return;
            }
        }
    }

    // handle session related headers too i think



    // check if the client requested to close the persistent connection
    if (req.headers.count("connection") != 0) {
        std::vector<std::string>& connection_values = req.headers["connection"];  
        // multiple connection values are stored
        if (connection_values.size() > 1) {
            construct_error_response(400);
            return;
        } else if (connection_values.size() == 1 && connection_values.at(0) == "close") {
            close_connection = true;
        }
    }
}


void Client::parse_req_line(std::string& req_line)
{
    // preliminary validation - request line does NOT have 3 components
    if (req_line.size() != 3) {
        construct_error_response(400);
        return;
    }
    req.req_method = req_line.at(0);
    req.path = req_line.at(1);
    req.version = req_line.at(2);

    // unsupported http version
    if (req.version != HttpServer::version) {
        construct_error_response(505);
        return;
    }

    // unsupported method
    if (HttpServer::supported_methods.count(req.req_method) == 0) {
        construct_error_response(501);
        return;    
    }
}


void Client::parse_headers(std::vector<std::string> headers)
{
    for (std::string header : headers) {
        std::vector<std::string> header_components = Utils::split_on_first_delim(header, ":");
        // malformed header - header type and value(s) are not separated by ":"
        if (header_components.size() != 2) {
            construct_error_response(400);
            break;
        }
        std::string header_key = Utils::to_lowercase(header_components.at(0));
        std::string header_values = Utils::trim(header_components.at(1));

        // split header_values on ",", trim each value, and add it to the vector for the header_key
        std::vector<std::string> header_values_vec = Utils::split(header_values, ",");
        for (std::string& value : header_values_vec) {
            value = Utils::trim(value);
            req.headers[header_key].push_back(value);
        }
    }

    // host header not present
    if (req.headers.count("host") == 0) {
        construct_error_response(400);
        return;
    }
}


void Client::set_request_type()
{
    // loop through server's routing table and check if the method + route matches any of the entries
    // if so, set is_static to false
    // ! implement this loop for dynamic requests

    // Additional error handling if request is static
    if (req.is_static) {
        // set resource path for current request
        req.static_resource_path = HttpServer::static_dir + "/" + req.path;

        // verify that the file can be opened
        std::ifstream resource(req.static_resource_path);
        if (resource.is_open()) {
            construct_error_response(404);
            return;    
        }
        resource.close();

        // check if user is trying to access server files
        if (req.static_resource_path.find("..") != std::string::npos) {
            construct_error_response(403);
            return;    
        }

        // only GET or HEAD allowed for static requests
        if (req.req_method == "POST" || req.req_method == "PUT") {
            construct_error_response(405);
            return;    
        }

        // ! handle if modified since header
    }
}


void Client::construct_error_response(int err_code)
{
    res.code = err_code;
    res.reason = HttpServer::response_codes.at(err_code);



    // response is ready to send back to client
    response_ready = true;
}


void Client::construct_response()
{
    res.code = 200;
    res.reason = HttpServer::response_codes.at(200);

    res.headers["Server"] = {"5050-Web-Server/1.0"};

    // static response
    if (req.is_static) {
        // set content-type header

        // set content-length header


        // write body if NOT a head request
        if (req.req_method != "HEAD") {

        }
    } 
    // dynamic response
    else {

    }

    // response is ready to send back to client
    response_ready = true;
}


void Client::send_response()
{
    std::ostringstream response_msg;
    response_msg << res.version << " " << std::to_string(res.code) << " " << res.reason << CRLF;
    for (const auto& entry : res.headers) {
        for (std::string value : entry.second) {
            response_msg << entry.first << ":" << value << CRLF;
        }
    }

    // Write body if present
    if (!res.body.empty()) {
        response_msg.write(res.body.data(), res.body.size());
        response_msg << CRLF;
    }

    response_msg << CRLF;

    int response_msg_len = response_msg.str().length();
    int total_bytes_sent = 0;
    while (total_bytes_sent < response_msg_len) {
        int bytes_sent = send(client_fd, response_msg.str().c_str(), response_msg_len, 0);
        if (bytes_sent == -1) {
            // // Utils::error("Unable to send response");
            continue;
        }
        total_bytes_sent += bytes_sent;
    }
}
