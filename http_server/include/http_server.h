#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

struct RouteTableEntry {
    std::string method;
    std::string path;
    // std::function<void(const HttpRequest&, HttpResponse&)> route;  // ! check if this field is okay

    // constructor
    // RouteTableEntry(const std::string& method, const std::string& path,
    //                 const std::function<void(const HttpRequest&, HttpResponse&)>& route)
    //     : method(method), path(path), route(route) {}
    RouteTableEntry(const std::string& method, const std::string& path)
        : method(method), path(path) {}
    // delete default constructor
    RouteTableEntry() = delete;
};

class HttpServer 
{
// fields
public:
    // constants
    static const std::string version;                                   // HTTP version (HTTP/1.1)
    static const std::unordered_set<std::string> supported_methods;     // GET, HEAD, POST, PUT
    static const std::unordered_map<int, std::string> response_codes;   // Response codes and associated message (Ex. 200 OK)

    // server fields
    static int port;                   // port server runs on
    static std::string static_dir;     // location of static files that server may wish to serve

private:
    static int server_sock_fd;                             // bound server socket's fd
    static std::vector<RouteTableEntry> routing_table;     // routing table entries for server - order in which routes are registered matters when matching routes


// methods
public:    
    static void run(int port);                           // run server (server does NOT run on initialization, server instance must explicitly call this method)
    static void run(int port, std::string static_dir);   // overload run if user wants to run server with custom static file dir

    // route handlers
    // static void get(std::string path, std::function<void(const HttpRequest&, HttpResponse&)>);     // register GET route with handler
    // static void put(std::string path, std::function<void(const HttpRequest&, HttpResponse&)>);     // register PUT route with handler
    // static void post(std::string path, std::function<void(const HttpRequest&, HttpResponse&)>);    // register POST route with handler
    static void get(std::string path);     // register GET route with handler
    static void put(std::string path);     // register PUT route with handler
    static void post(std::string path);    // register POST route with handler

private:
    // make default constructor private
    HttpServer() {}

    static int bind_server_socket();           // bind port to socket
    static void accept_and_handle_clients();   // main server loop to accept and handle clients
};

#endif