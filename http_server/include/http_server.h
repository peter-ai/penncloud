#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <unordered_set>

class HttpServer 
{
// fields
public:
    // constants
    static const std::string version;                                   // HTTP version (HTTP/1.1)
    static const std::unordered_set<std::string> supported_methods;     // GET, HEAD, POST, PUT
    static const std::unordered_map<int, std::string> response_codes;   // Response codes and associated message (Ex. 200 OK)

    // instance fields
    int port;                   // port server runs on
    std::string static_dir;     // location of static files that server may wish to serve

private:
    int server_sock_fd;                           // bound server socket's fd
    std::vector<RouteTableEntry> routing_table;     // routing table entries for server - order in which routes are registered matters

// methods
public:
    // constructors
    // initialize server with port and default static directory
    HttpServer(int port) : port(port),server_sock_fd(-1),static_dir("static") {}
    // initialize server with port and user-specified static directory
    HttpServer(int port, std::string static_dir) : port(port),server_sock_fd(-1),static_dir(static_dir) {}
    // disable default constructor - HttpServer must be initialized with a port
    HttpServer() = delete; 

    // methods
    void run();   // run server (server does NOT run on initialization, server instance must explicitly call this method)

    // route handlers
    void get(std::string path, std::function<void(const HttpRequest&, HttpResponse&)>);     // register GET route with handler
    void put(std::string path, std::function<void(const HttpRequest&, HttpResponse&)>);     // register PUT route with handler
    void post(std::string path, std::function<void(const HttpRequest&, HttpResponse&)>);    // register POST route with handler

private:
    int bind_server_socket();           // bind port to socket
    void accept_and_handle_clients();   // main server loop to accept and handle clients
};


struct RouteTableEntry {
    std::string method;
    std::string path;
    std::function<void(const HttpRequest&, HttpResponse&)> route;  // ! check if this field is okay

    // constructor
    RouteTableEntry(const std::string& method, const std::string& path,
                    const std::function<void(const HttpRequest&, HttpResponse&)>& route)
        : method(method), path(path), route(route) {}
    // delete default constructor
    RouteTableEntry() = delete;
};


#endif