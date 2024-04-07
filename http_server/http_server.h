#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <iostream>

class HttpServer 
{
// fields
private:
    int m_port;           // port server runs on
    int m_server_sock_fd; // bound server socket's fd 

// methods
public:
    // server initialized with a port
    HttpServer(int port) : m_port(port) {}
    // disable default constructor - HttpServer must be initialized with a port
    HttpServer() = delete; 

    void run();   // run server

    // route handlers
    void get(std::string path);      // register GET route with handler
    void put(std::string path);      // register PUT route with handler
    void post(std::string path);     // register POST route with handler

private:
    int bind_server_socket();         // bind port to socket
    void accept_and_handle_clients(); // main server loop to accept and handle clients
};

#endif