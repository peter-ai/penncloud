#ifndef BE_UTILS_H
#define BE_UTILS_H

#include <string>
#include <vector>

namespace BeUtils
{
    // Create a socket and open a connection to the specified port. Returns a file descriptor.
    int open_connection(int port);

    // Coordinator and other communication is separate because coordinator uses \r\n as delimiter in communication

    // Write message to fd
    int write(const int fd, std::vector<char> &response_msg);
    // Read message from fd
    std::string read(int fd);

    // Coordinator communication
    // Write message to coordinator
    int write_to_coord(std::string &msg);
    // Read message from coordinator
    std::string read_from_coord();
}

#endif