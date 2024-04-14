#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include <string>
#include <unordered_set>

class BackendServer 
{
// fields
public:
    // constants
    // GET, PUT
    static const std::unordered_set<std::string> supported_commands;

    // backend server fields
    static int coord_port;             // port coordinator is running on
    static int port;                   // port server runs on
    // NOTE: if the key range is "aa" to "bz", this server will manage every key UP TO AND INCLUDING "bz"
    // For example, a key called bzzzz would be managed in this server. The next server would start at "ca" 
    static std::string range_start;     // start of key range managed by this backend server
    static std::string range_end;       // end of key range managed by this backend server
  
private:
    static int server_sock_fd;       // bound server socket's fd

// methods
public:    
    static void run();                           // run server (server does NOT run on initialization, server instance must explicitly call this method)

private:
    // make default constructor private
    BackendServer() {}

    static int bind_server_socket();           // bind port to socket
    static void initialize_tablets();          // initialize tablets on this server
    static void send_coordinator_heartbeat();  // 
    static void accept_and_handle_clients();   // main server loop to accept and handle clients
};

#endif