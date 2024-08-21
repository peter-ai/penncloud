#include "../include/loadbalancer.h"

// main method that run load balancer
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <number_of_servers>" << std::endl;
        return 1;
    }

    int numServers = std::stoi(argv[1]);
    int startingPort = 8000; // Default starting port

    // 1. Populate map of servers and server mutexes
    LoadBalancer::initialize_servers(numServers, startingPort);
    // Send list of FE servers to Admin Console that sits on port 8080
    LoadBalancer::lb_to_admin(8080);

    // 2. Register GET route to “/” using HttpServer
    HttpServer::get("*", LoadBalancer::client_handler); // register handler

    LoadBalancer::client_listen_port = 7500; // Port for client connections
    LoadBalancer::server_listen_port = 4000; // Port for server heartbeats and registration

    // 3. Create thread and pass ping function (receive heartbeats) into it
    std::thread heartbeatThread(LoadBalancer::receive_heartbeat);

    // Detach the heartbeat thread
    heartbeatThread.detach();

    // 4. Create threads and pass health check (check if server is alive)
    std::thread healthcheckThread(LoadBalancer::health_check);

    // Detach the health check thread
    healthcheckThread.detach();

    // 5. Call HttpServer::run(port_to_listen_to_clients_on)
    HttpServer::run(LoadBalancer::client_listen_port);

    return 0;
}