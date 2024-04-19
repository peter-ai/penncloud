#ifndef BE_UTILS_H
#define BE_UTILS_H

#include <string>

namespace BeUtils
{
    // Create a socket and open a connection to the specified port. Returns a file descriptor.
    int BeUtils::open_connection(int port);
    // Write message to fd
    int BeUtils::write(const int fd, const std::string &msg);
    // Read message from fd
    std::string BeUtils::read(int fd);
}

#endif