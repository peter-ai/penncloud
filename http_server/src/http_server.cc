#include <iostream>
#include <sys/socket.h>   // socket
#include <netinet/in.h>   // sockaddr_in
#include <thread>

#include "http_server.h"
#include "client.h"
#include "../utils/include/utils.h"

// initialize constant members
const std::string HttpServer::version = "HTTP/1.1";
const std::unordered_set<std::string> HttpServer::supported_methods = {"GET", "HEAD", "POST", "PUT"};
const std::unordered_map<int, std::string> HttpServer::response_codes = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {501, "Not Implemented"},
    {505, "HTTP Version Not Supported"}
};

// initialize static members to dummy or default values
int HttpServer::port = -1;
int HttpServer::server_sock_fd = -1;
std::string HttpServer::static_dir = "";
std::vector<RouteTableEntry> HttpServer::routing_table;

// http server logger
Logger http_logger("HTTP Server");

void HttpServer::run(int port, std::string static_dir)
{
    HttpServer::port = port;
    HttpServer::static_dir = static_dir;
    if (bind_server_socket() < 0) {
        http_logger.log("Failed to initialize server. Exiting.", 40);
        return;
    }
    http_logger.log("HTTP server listening for connections on port " + std::to_string(HttpServer::port), 20);
    http_logger.log("Serving static files from " + HttpServer::static_dir + "/", 20);
    accept_and_handle_clients();
}


void HttpServer::run(int port)
{
    HttpServer::run(port, "static");
}


// void HttpServer::get(std::string path, std::function<void(const HttpRequest&, HttpResponse&)> route) 
// {
//     // every GET request is also a valid HEAD request
//     RouteTableEntry get_entry("GET", path, route);
//     RouteTableEntry head_entry("HEAD", path, route);
//     HttpServer::routing_table.push_back(get_entry);
//     HttpServer::routing_table.push_back(head_entry);
// }


// void HttpServer::put(std::string path, std::function<void(const HttpRequest&, HttpResponse&)> route) 
// {
//     RouteTableEntry entry("PUT", path, route);
//     HttpServer::routing_table.push_back(entry);
// }


// void HttpServer::post(std::string path, std::function<void(const HttpRequest&, HttpResponse&)> route) 
// {
//     RouteTableEntry entry("POST", path, route);
//     HttpServer::routing_table.push_back(entry);
// }

void HttpServer::get(std::string path) 
{
    // every GET request is also a valid HEAD request
    RouteTableEntry get_entry("GET", path);
    RouteTableEntry head_entry("HEAD", path);
    HttpServer::routing_table.push_back(get_entry);
    HttpServer::routing_table.push_back(head_entry);
}


void HttpServer::put(std::string path) 
{
    RouteTableEntry entry("PUT", path);
    HttpServer::routing_table.push_back(entry);
}


void HttpServer::post(std::string path) 
{
    RouteTableEntry entry("POST", path);
    HttpServer::routing_table.push_back(entry);
}


int HttpServer::bind_server_socket()
{
    // create server socket
    if ((HttpServer::server_sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        http_logger.log("Unable to create server socket.", 40);
        return -1;
    }

    // bind server socket to server's port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    int opt = 1;
    if ((setsockopt(HttpServer::server_sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0) {
        http_logger.log("Unable to reuse port to bind server socket.", 40);
        return -1;
    }

    if ((bind(HttpServer::server_sock_fd, (const sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
        http_logger.log("Unable to bind server socket to port.", 40);
        return -1;
    }

    // listen for connections on port
    // ! check if this value is okay
    const int BACKLOG = 20;
    if ((listen(HttpServer::server_sock_fd, BACKLOG)) < 0) {
        http_logger.log("Unable to listen for client connections.", 40);
        return -1;
    }
    return 0;
}


void HttpServer::accept_and_handle_clients()
{
    while (true) {
        // accept client connection, which returns a fd for the client
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        if ((client_fd = accept(HttpServer::server_sock_fd, (sockaddr*) &client_addr, &client_addr_size)) < 0) {
            http_logger.log("Unable to accept incoming connection from client. Skipping.", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }

        Client client(client_fd);
        http_logger.log("Accepted connection from client __________", 20);

        // launch thread to handle client
        std::thread client_thread(&Client::read_from_network, &client);
        // ! fix this after everything works (manage multithreading)
        client_thread.detach();
    }
}