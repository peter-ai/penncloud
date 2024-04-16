#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>
#include <sys/fcntl.h> // for non-blocking sockets

constexpr int BUFFER_SIZE = 1024;
int server_socket;

// Function to handle client connections
// Function to handle client connections
void handle_client(int client_socket) {
    // Set the socket to non-blocking
    fcntl(client_socket, F_SETFL, O_NONBLOCK);

    char buffer[BUFFER_SIZE];
    std::vector<char> client_data;

    while (true) {
        memset(buffer, 0, BUFFER_SIZE); // Clear buffer
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (bytes_received > 0) {
            // Append data to the vector
            client_data.insert(client_data.end(), buffer, buffer + bytes_received);

            // Log received data
            std::cout << "Received data from client: " << std::string(buffer, bytes_received) << std::endl;

            // Construct response
            std::string response = "+OK ";
            if (client_data.size() >= 4) {
                response.append(client_data.begin(), client_data.begin() + 4); // First 4 characters of the received message
            }

            uint32_t response_size = htonl(response.size());
            std::vector<char> response_data(sizeof(uint32_t) + response.size());
            memcpy(response_data.data(), &response_size, sizeof(uint32_t));
            memcpy(response_data.data() + sizeof(uint32_t), response.c_str(), response.size());

            // Send the size-prefixed response
            size_t total_bytes_sent = 0;
            while (total_bytes_sent < response_data.size()) {
                ssize_t bytes_sent = send(client_socket, response_data.data() + total_bytes_sent, response_data.size() - total_bytes_sent, 0);
                if (bytes_sent == -1) {
                    std::cerr << "Error sending response to client" << std::endl;
                    close(client_socket);
                    break;
                }
                total_bytes_sent += bytes_sent;
            }
            std::cerr << "Sent response to client." << std::endl;
            std::cerr << "bytes sent = " << total_bytes_sent << std::endl;
            
        } else if (bytes_received == 0) {
            std::cout << "Client disconnected" << std::endl;
            break; // Break the loop if client disconnected
        } else if (bytes_received == -1) {
            if (errno != EWOULDBLOCK) {
                std::cerr << "Error receiving data from client" << std::endl;
                break;
            }
        }
        // Yield processor time, to simulate some processing and avoid busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // close(client_socket);
}

// Signal handler for SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    std::cout << "Received SIGINT, closing server socket" << std::endl;
    close(server_socket);
    exit(0);
}

int main() {
    // Register SIGINT handler
    signal(SIGINT, sigint_handler);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Error creating server socket" << std::endl;
        return 1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8000);

    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        std::cerr << "Error setting socket options" << std::endl;
        close(server_socket);
        return 1;
    }

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        std::cerr << "Error binding server socket" << std::endl;
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        std::cerr << "Error listening on server socket" << std::endl;
        close(server_socket);
        return 1;
    }

    while (true) {
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket == -1) {
            std::cerr << "Error accepting connection" << std::endl;
            continue; // Continue to accept next connection
        }

        // Spin up a thread to handle the client connection
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach(); // Detach the thread to allow it to run independently
    }

    close(server_socket);
    return 0;
}
