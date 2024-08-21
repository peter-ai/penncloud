#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <iostream>
#include <sys/socket.h> // socket
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>  // inet_pton
#include <thread>

#include "client.h"
#include "http_request.h"
#include "http_response.h"
#include "../../utils/include/utils.h"

#include "../../loadbalancer/include/loadbalancer.h"

struct RouteTableEntry
{
    std::string method;
    std::string path;
    std::function<void(const HttpRequest &, HttpResponse &)> route;

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
    static int admin_port;                             // port admin connections are serviced on
    static std::string static_dir;                     // location of static files that server may wish to serve
    static std::vector<RouteTableEntry> routing_table; // routing table entries for server - order in which routes are registered matters when matching routes
    static std::atomic<bool> is_dead;                  // tracks if the server is currently dead (from an admin kill command)

    // active connection fields (clients)
    static std::unordered_map<pthread_t, std::atomic<bool>> client_connections;
    static std::mutex client_connections_lock;

private:
    static std::shared_timed_mutex kvs_mutex;                                              // mutex for client kvs addresses
    static std::unordered_map<std::string, std::vector<std::string>> client_kvs_addresses; // map for user kvs addresses

    // methods
public:
    static void run(int port);                         // run server (server does NOT run on initialization, server instance must explicitly call this method)
    static void run(int port, std::string static_dir); // overload run if user wants to run server with custom static file dir

    // route handlers
    static void get(const std::string &path, const std::function<void(const HttpRequest &, HttpResponse &)> &route);  // register GET route with handler
    static void post(const std::string &path, const std::function<void(const HttpRequest &, HttpResponse &)> &route); // register POST route with handler

    // Admin communication
    static int dispatch_admin_listener_thread();                 // dispatch thread to read from admin
    static void accept_and_handle_admin_comm(int admin_sock_fd); // open connection with admin port and read messages
    static void admin_kill();                                    // handles kill command from admin console
    static void admin_live();                                    // handles live command from admin console

    // helper functions to safely access client kvs addresses
    static bool check_kvs_addr(std::string username);
    static bool delete_kvs_addr(std::string username);
    static std::vector<std::string> get_kvs_addr(std::string username);
    static bool set_kvs_addr(std::string username, std::string kvs_address);

    // send heartbeat to LOAD BALANCER
    static void start_heartbeat_thread(int lb_port, int server_port);
    static void send_heartbeat(int lb_port, int server_port);

private:
    // make default constructor private
    HttpServer() {}

    static int bind_socket(int port);        // bind port to socket
    static void accept_and_handle_clients(); // main server loop to accept and handle clients
};

#endif