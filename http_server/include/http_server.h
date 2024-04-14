#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "http_request.h"
#include "http_response.h"

struct RouteTableEntry
{
    std::string method;
    std::string path;
    std::function<void(const HttpRequest &, HttpResponse &)> route; // ! check if this field is okay

    // constructor
    RouteTableEntry(const std::string &method, const std::string &path,
                    const std::function<void(const HttpRequest &, HttpResponse &)> &route)
        : method(method), path(path), route(route) {}
    // delete default constructor
    RouteTableEntry() = delete;
};

class HttpServer
{
    // fields
public:
    // constants
    static const std::string version;                               // HTTP version (HTTP/1.1)
    static const std::unordered_set<std::string> supported_methods; // GET, HEAD, POST, PUT

    // server fields
    static int port;                                   // port server runs on
    static std::string static_dir;                     // location of static files that server may wish to serve
    static std::vector<RouteTableEntry> routing_table; // routing table entries for server - order in which routes are registered matters when matching routes

private:
    static int server_sock_fd;                                                             // bound server socket's fd
    static std::mutex kvs_mutex;                                                     // mutex for client kvs addresses
    static std::unordered_map<std::string, std::vector<std::string>> client_kvs_addresses; // map for user kvs addresses

    // methods
public:
    static void run(int port);                         // run server (server does NOT run on initialization, server instance must explicitly call this method)
    static void run(int port, std::string static_dir); // overload run if user wants to run server with custom static file dir

    // route handlers
    static void get(const std::string &path, const std::function<void(const HttpRequest &, HttpResponse &)> &route);  // register GET route with handler
    static void post(const std::string &path, const std::function<void(const HttpRequest &, HttpResponse &)> &route); // register POST route with handler

    // ! add function to safely accesss user_backend_address map
    // ! return an empty vector to frontend
    // ! make sure all of these are thread safe
    static bool check_kvs_addr(std::string username);
    static std::vector<std::string> get_kvs_addr(std::string username); // ! check if user exists before
    static bool delete_kvs_addr(std::string username);     // ! delete user
    static bool set_kvs_addr(std::string username, std::string backend_address);
    // ! within add, check if the user already exists in the map, if they do, replace their existing value. If not, add a new entry

private:
    // make default constructor private
    HttpServer() {}

    static int bind_server_socket();         // bind port to socket
    static void accept_and_handle_clients(); // main server loop to accept and handle clients
};

#endif