#include <iostream>
#include <sys/socket.h>   // socket
#include <netinet/in.h>   // sockaddr_in
#include <thread>

#include "http_server.h"
#include "client.h"
#include "utils.h"

void HttpServer::run()
{
    if (bind_server_socket() < 0) {
        Utils::error("Failed to run server. Exiting.");
        return;
    }
    m_running = true;
    accept_and_handle_clients();
}


int HttpServer::bind_server_socket()
{
    // create server socket
    struct sockaddr_in server_addr;
    if ((m_server_sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        Utils::error("Unable to create server socket.");
        return -1;
    }

    // bind server socket to server's port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_port);
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);

    int opt = 1;
    if ((setsockopt(m_server_sock_fd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt))) < 0) {
        Utils::error("Unable to reuse port to bind server socket.");
        return -1;
    }

    if ((bind(m_server_sock_fd, (const sockaddr*) &server_addr, sizeof(server_addr))) < 0) {
        Utils::error("Unable to bind server socket to port.");
        return -1;
    }

    // listen for connections on port
    // ! check if this value is okay
    const int BACKLOG = 20;
    if ((listen(m_server_sock_fd, BACKLOG)) < 0) {
        Utils::error("Unable to listen for connections.");
        return -1;
    }

    std::cout << "Server listening for connections on port " << m_port << std::endl;
    return 0;
}


void HttpServer::accept_and_handle_clients()
{
  while (true) {
    // accept client connection, which returns a different fd for the client connection
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    if ((client_fd = accept(m_server_sock_fd, (sockaddr*) &client_addr, &client_addr_size)) < 0) {
        Utils::error("Unable to accept incoming connection from client. Skipping.");
        // error with incoming connection should NOT break the server loop
        continue;
    }

    Client client(client_fd);
    // launch thread to handle client
    std::thread client_thread(&Client::read_from_network, &client);
  }
}