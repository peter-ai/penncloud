#include <iostream>
#include <sys/socket.h>   // socket
#include <netinet/in.h>   // sockaddr_in
#include <thread>

#include "http_server.h"
#include "client.h"
#include "utils.h"

const std::string HttpServer::version = "HTTP/1.1";
const std::unordered_set<std::string> HttpServer::supported_methods = {"GET", "HEAD", "POST", "PUT"};
const std::unordered_map<int, std::string> HttpServer::response_codes = {
    {200, "OK"},
    {400, "Bad Request"},
    {404, "Not Found"},
    {501, "Not Implemented"},
    {505, "HTTP Version Not Supported"}
};


void HttpServer::run()
{
    if (bind_server_socket() < 0) {
        Utils::error("Failed to run server. Exiting.");
        return;
    }
    accept_and_handle_clients();
}


void HttpServer::get(std::string path, std::function<void(const HttpRequest&, HttpResponse&)> route) 
{
    // every GET request is also a valid HEAD request
    RouteTableEntry get_entry("GET", path, route);
    RouteTableEntry head_entry("HEAD", path, route);
    routing_table.push_back(get_entry);
    routing_table.push_back(head_entry);
}


void HttpServer::put(std::string path, std::function<void(const HttpRequest&, HttpResponse&)> route) 
{
    RouteTableEntry entry("PUT", path, route);
    routing_table.push_back(entry);
}


void HttpServer::post(std::string path, std::function<void(const HttpRequest&, HttpResponse&)> route) 
{
    RouteTableEntry entry("POST", path, route);
    routing_table.push_back(entry);
}


int HttpServer::bind_server_socket()
{
    // create server socket
    struct sockaddr_in server_addr;
    if ((server_sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        Utils::error("Unable to create server socket.");
        return -1;
    }

    // bind server socket to server's port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    int opt = 1;
    if ((setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt))) < 0) {
        Utils::error("Unable to reuse port to bind server socket.");
        return -1;
    }

    if ((bind(server_sock_fd, (const sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
        Utils::error("Unable to bind server socket to port.");
        return -1;
    }

    // listen for connections on port
    // ! check if this value is okay
    const int BACKLOG = 20;
    if ((listen(server_sock_fd, BACKLOG)) < 0) {
        Utils::error("Unable to listen for connections.");
        return -1;
    }

    std::cout << "HTTP server listening for connections on port " << std::to_string(port) << std::endl;
    return 0;
}


void HttpServer::accept_and_handle_clients()
{
    while (true) {
        // accept client connection, which returns a fd for the client
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        if ((client_fd = accept(server_sock_fd, (sockaddr*) &client_addr, &client_addr_size)) < 0) {
            Utils::error("Unable to accept incoming connection from client. Skipping.");
            // error with incoming connection should NOT break the server loop
            continue;
        }

        std::cout << "Accepted connection from client __________" << std::endl;
        Client client(client_fd);
        // launch thread to handle client
        std::thread client_thread(&Client::read_from_network, &client);
    }
}