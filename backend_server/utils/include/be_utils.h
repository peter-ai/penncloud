#ifndef BE_UTILS_H
#define BE_UTILS_H

#include <string>
#include <vector>

namespace BeUtils
{
    // Open a connection to the specified port
    int open_connection(int port, int fd);

    // Coordinator and other communication is separate because coordinator uses \r\n as delimiter in communication

    // Write message prepended with size prefix to fd
    int write(const int fd, std::vector<char> &msg);
    // Read byte stream from fd
    std::vector<char> read(int fd);

    // Coordinator communication
    // Write message to coordinator
    int write_to_coord(int fd, std::string &msg);
    // Read message from coordinator
    std::string read_from_coord(int fd);

    // host number <-> network vector conversion
    std::vector<uint8_t> host_num_to_network_vector(uint32_t num);
    uint32_t network_vector_to_host_num(std::vector<char> &num_vec);
}

#endif