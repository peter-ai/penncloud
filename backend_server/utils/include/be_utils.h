#ifndef BE_UTILS_H
#define BE_UTILS_H

#include <string>
#include <vector>

namespace BeUtils
{
    int open_connection(int port); // Open a connection to the specified port. Returns a fd if successful.

    // Write message to coordinator (supply fd to coordinator in both cases)
    int write_to_coord(int fd, std::string &msg); // Write message to coordinator
    std::string read_from_coord(int fd);          // Read message from coordinator

    int write(const int fd, std::vector<char> &msg); // Write message prepended with size prefix to fd
    std::vector<char> read(int fd);                  // Read byte stream from fd

    // host number <-> network vector conversion
    std::vector<uint8_t> host_num_to_network_vector(uint32_t num);
    uint32_t network_vector_to_host_num(std::vector<char> &num_vec);
}

#endif