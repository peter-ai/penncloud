#include <iostream>
#include <sys/socket.h>   // recv
#include <unistd.h>       // close
#include <fstream>
#include <sstream>

#include "client.h"
#include "http_request.h"
#include "http_response.h"
#include "http_server.h"
#include "../../utils/include/utils.h"

const std::string Client::CRLF = "\r\n";
const std::string Client::DOUBLE_CRLF = "\r\n\r\n";

Logger http_client_logger("HTTP Client");

void Client::read_from_network()
{
    std::string client_stream;
    int bytes_recvd;
    while (true) {
        char buf[4096];
        bytes_recvd = recv(client_fd, buf, 4096, 0);
        if (bytes_recvd < 0) {
            http_client_logger.log("Error reading from client", 40);
            break;
        } else if (bytes_recvd == 0) {
            http_client_logger.log("Client closed connection", 30);
            break;
        }

        for (int i = 0 ; i < bytes_recvd ; i++) {
            // byte is part of request body
            if (remaining_body_len > 0) {
                req.body.push_back(buf[i]);
                remaining_body_len--;

                // prev req is complete - prepare and send response
                if (remaining_body_len == 0) {
                    handle_req();
                    if (!response_ready) {
                        construct_response();
                    }
                    send_response();
                    if (close_connection) {
                        break;
                    }
                }
            } 
            // byte is part of client_stream (building request)            
            else {
                client_stream += buf[i];
                // double CRLF indicates a full http request WITHOUT the body (if present)
                if (client_stream.length() >= 4 && client_stream.substr(client_stream.length() - 4) == DOUBLE_CRLF) {
                    // build http request from client stream and then clear current stream
                    parse_req(client_stream);
                    client_stream.clear();

                    // request had content-length of 0
                    if (remaining_body_len == 0) {
                        handle_req();
                        if (!response_ready) {
                            construct_response();
                        }
                        send_response();
                        if (close_connection) {
                            break;
                        }
                    }
                }
            }
        }

        if (close_connection) {
            break;
        }
    }
    close(client_fd);
}


void Client::parse_req(std::string& client_stream) 
{
    std::vector<std::string> msg_lines = Utils::split(client_stream, CRLF);

    parse_req_line(msg_lines.at(0));
    // Error occurred while parsing req line - this is the only acceptable early exit
    if (response_ready) {
        return;
    }

    msg_lines.erase(msg_lines.begin());
    parse_headers(msg_lines);

    // check if http message has a body
    std::vector<std::string> content_length_vals = req.get_header("content-length");
    if (content_length_vals.size() != 0) {
        // multiple content length values are stored
        if (content_length_vals.size() > 1) {
            http_client_logger.log("(400) Multiple values for content length are not allowed", 40);
            construct_error_response(400);
            return;
        } else if (content_length_vals.size() == 1) {
            // content length value is not a valid integer
            try {
                remaining_body_len = std::stoi(content_length_vals.at(0));
            } catch (const std::exception& e) {
                http_client_logger.log("(400) Non-numerical content length provided", 40);
                construct_error_response(400);
                return;
            }
        }
    }
}


void Client::parse_req_line(std::string& req_line)
{
    std::vector<std::string> req_line_components = Utils::split(req_line, " ");

    // preliminary validation - request line does NOT have 3 components
    if (req_line_components.size() != 3) {
        http_client_logger.log("(400) Malformed request line", 40);
        construct_error_response(400);
        return;
    }
    req.method = req_line_components.at(0);
    req.path = req_line_components.at(1);
    req.version = req_line_components.at(2);
}


void Client::parse_headers(std::vector<std::string>& headers)
{
    for (std::string& header : headers) {
        std::vector<std::string> header_components = Utils::split_on_first_delim(header, ":");

        // malformed header - header type and value(s) are not separated by ":"
        if (header_components.size() != 2) {
            http_client_logger.log("(400) Malformed header", 40);
            construct_error_response(400);
            // do not return, since content length header may still need to be parsed
            // if content length header is not provided or malformed, then behavior is undefined
            continue;
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
    if (req.get_header("host").size() == 0) {
        http_client_logger.log("(400) No host header", 40);
        construct_error_response(400);
    }
}


void Client::handle_req() {
    // unsupported http version
    if (req.version != HttpServer::version) {
        http_client_logger.log("(505) Unsupported HTTP version", 40);
        construct_error_response(505);
        return;
    }

    // unsupported method
    if (HttpServer::supported_methods.count(req.method) == 0) {
        http_client_logger.log("(501) Unsupported method", 40);
        construct_error_response(501);
        return;    
    }

    // check if request is static or dynamic (along with associated validation for req type)
    set_req_type();

    // check if the client requested to close the persistent connection - should be checked even in bad request as long as req line + headers are valid
    // note that default behavior is a persistent connection, so Connection: close is the only header that will cause the server to explicitly terminate the connection
    std::vector<std::string> connection_vals = req.get_header("connection");
    if (connection_vals.size() != 0) {
        // multiple connection values are stored
        if (connection_vals.size() > 1) {
            http_client_logger.log("(400) Multiple values for connection header are not allowed", 40);
            construct_error_response(400);
            return;
        } else if (connection_vals.at(0) == "close") {
            close_connection = true;
        }
    }
}


void Client::set_req_type()
{
    // parse query parameters
    size_t param_start = req.path.find_first_of('?');
    if (param_start != std::string::npos) {
        std::vector<std::string> split_params = Utils::split(req.path.substr(param_start + 1), "&");
        for (std::string param : split_params) {
            std::vector<std::string> query_key_val = Utils::split(param, "=");
            if (query_key_val.size() == 2) {
                req.query_params[query_key_val[0]] = query_key_val[1];
            }
        }
        // update path if query parameters found in request
        // if path was /some_path?abc=123, it'll now be /some_path? (which should match a dynamic route)
        req.path = req.path.substr(0, param_start + 1);
    }

    // match incoming request path to stored dynamic routes
    std::vector<std::string> req_path_tokens = Utils::split(req.path, "/");

    for (RouteTableEntry& route : HttpServer::routing_table) {
        // methods must match for the request to match the route
        if (route.method != req.method) {
            continue;
        }

        std::vector<std::string> route_path_tokens = Utils::split(route.path, "/");

        // iterate route path tokens and check if there's a match for each token
        bool dynamic_route_found = true;
        bool wildcard_found = false;
        for (size_t i = 0 ; i < route_path_tokens.size() ; i++) {
            std::string req_token = req_path_tokens.at(i);
            std::string route_token = route_path_tokens.at(i);

            // tokens are a direct match OR
            // route token starts with ":" which matches any value
            if (req_token == route_token || route_token.front() == ':') {
                continue;
            } 
            // route token is *, in which case we're done and we can break since * catches everything
            else if (route_token == "*") {
                wildcard_found = true;
                break;
            } else {
                dynamic_route_found = false;
            }
        }

        // check that route sizes matched up only if wildcard was not found
        if (!wildcard_found && req_path_tokens.size() != route_path_tokens.size()) {
            continue;
        }

        if (dynamic_route_found) {
            req.dynamic_route = route.route;
            req.is_static = false;
            break;
        }
    }

    // Error handling if request is static
    if (req.is_static) {
        // standard size of path if necessary
        if (req.path.front() != '/') {
            req.path = "/" + req.path;
        }

        // set resource path for current request
        req.static_resource_path = HttpServer::static_dir + req.path;

        // verify that the file can be opened (file exists)
        std::ifstream resource(req.static_resource_path);
        if (!resource.is_open()) {
            http_client_logger.log("(404) Failed to open static file", 40);
            construct_error_response(404);
            return;    
        }
        resource.close();

        // check if user is trying to access server files
        if (req.static_resource_path.find("..") != std::string::npos) {
            http_client_logger.log("(403) Accessing forbidden files", 40);
            construct_error_response(403);
            return;    
        }

        // only GET or HEAD allowed for static requests
        if (req.method == "POST") {
            http_client_logger.log("(405) POST used for static request", 40);
            construct_error_response(405);
            return;    
        }

        // ! handle if modified since header
    } 
}


void Client::construct_error_response(int err_code)
{
    // necessary in case multiple errors occurred (first error supersedes errors found later)
    if (response_ready) {
        return;
    }
    res.set_code(err_code);
    std::string body = std::to_string(err_code) + " - " + res.reason;
    res.append_body_str(body);

    // response is ready to send back to client
    response_ready = true;
}


void Client::construct_response()
{
    // default response code indicating req is valid from server's end
    // This can be overridden by a dynamic request's route handler
    res.set_code(200);

    // static response
    if (req.is_static) {
        // set content-type header
        std::string content_type = "application/octet-stream"; // default content type
        size_t pos = req.path.find_last_of('.');
        if (pos != std::string::npos) {
            std::string extension = req.path.substr(pos + 1);
            if (extension == "txt") {
                content_type = "text/plain";
            } else if (extension == "jpeg" || extension == "jpg") {
                content_type = "image/jpeg";
            } else if (extension == "html") {
                content_type = "text/html";
            }
        }
        res.set_header("Content-Type", content_type);
        
        // write body if NOT a head request
        if (req.method != "HEAD") {        
            // open file in binary form and write bytes to body
            std::ifstream resource(req.static_resource_path, std::ios::binary);

            std::vector<char> buffer(1024);
            while (resource.read(buffer.data(), 1024)) {
                res.append_body_bytes(buffer.data(), resource.gcount());
            }
            res.append_body_bytes(buffer.data(), resource.gcount());
            resource.close();
        }
    } 
    // dynamic response
    else {
        req.dynamic_route(req, res);
    }

    // response is ready to send back to client
    response_ready = true;
}


void Client::send_response()
{
    // add standard headers to maintain consistent in responses
    res.set_header("Server", "5050-Web-Server/1.0"); // server identity
    res.set_header("Content-Length", std::to_string(res.body.size())); // content-length

    // log request metadata (reconstruct request line from parsed parameters to ensure correct parsing)
    std::string log_str = req.method + " " + req.path + " " + std::to_string(res.code) + " - " + std::to_string(res.body.size());
    if (req.is_static) {
        http_client_logger.log("[static] " + log_str, 20);
    } else {
        http_client_logger.log("[dynamic] " + log_str, 20);
    }

    std::ostringstream response_msg;

    // response line
    response_msg << res.version << " " << std::to_string(res.code) << " " << res.reason << CRLF;

    // headers
    for (const auto& entry : res.headers) {
        for (std::string value : entry.second) {
            response_msg << entry.first << ": " << value << CRLF;
        }
    }
    response_msg << CRLF;

    // Allocate buffer for complete response
    std::vector<char> response_buffer(response_msg.str().length() + res.body.size());
    std::memcpy(response_buffer.data(), response_msg.str().c_str(), response_msg.str().length());
    if (!res.body.empty()) {
        std::memcpy(response_buffer.data() + response_msg.str().length(), res.body.data(), res.body.size());
    }

    // write response to client as bytes
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < response_buffer.size()) {
        int bytes_sent = send(client_fd, response_buffer.data() + total_bytes_sent, response_buffer.size() - total_bytes_sent, 0);
        total_bytes_sent += bytes_sent;
    }

    // clear all fields related to transaction
    req.reset();
    res.reset();
    response_ready = false;
}
