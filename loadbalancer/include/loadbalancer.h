#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <tuple>
#include <list>
#include <random>
#include <map>
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"
#include "../../http_server/include/http_server.h"
#include "../../utils/include/utils.h"

namespace LoadBalancer
{
    struct ServerData
    {
        std::chrono::steady_clock::time_point lastHeartbeat; // value for last received heartbeat of server
        bool isActive;                                       // boolean if server is up/down
    };
    // Variables
    extern std::map<int, ServerData> servers;       // iterator for round-robin server selection, key is server port number
    extern std::vector<int> activeServers;                      // vector of active server port numbers
    extern std::map<int, std::mutex> serverMutexes; // map of server mutexes, key is server port number
    extern std::mutex server_mutex;                           // mutex for managing access to active servers
    extern int client_listen_port;                            // the port number on which the LB listens to client connections
    extern int server_listen_port;                            // the port number on which the LB listens to front end server connections

    // Functions
    int create_socket(int port);                               // Set up TCP socket bound to specified port
    void initialize_servers(int numServers, int startingPort); // initialize # of servers
    std::string select_server();                               // Select an active server using round-robin scheduling
    void receive_heartbeat();                                  // Receive and handle heartbeat from servers
    void health_check();                                       // Check health of all servers and mark them as active or inactive based on heartbeat

    // handlers
    void client_handler(const HttpRequest &request, HttpResponse &response);

    // admin console
    void lb_to_admin(int admin_port);
}

#endif // LOADBALANCER_H
