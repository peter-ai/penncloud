#ifndef BE_UTILS_H
#define BE_UTILS_H

#include <string>
#include <vector>

namespace BeUtils
{
    // Struct to hold both the byte stream and an error code
    struct ReadResult
    {
        std::vector<char> byte_stream;
        int error_code = 0; // default error code of 0 (no error), -1 otherwise
    };

    // connection methods
    int bind_socket(int port);     // Binds server socket to specified port. Returns a fd if successful, -1 otherwise.
    int open_connection(int port); // Open a connection with the specified port. Returns a fd if successful, -1 otherwise.

    // write methods
    int write_with_crlf(int fd, std::string &msg);       // Write message to coordinator
    int write_with_size(int fd, std::vector<char> &msg); // Write message prepended with size prefix to fd

    // read methods
    int wait_for_events(const std::vector<int> &fds, int timeout_ms); // Waits for event to occur on all fds within specified timeout
    ReadResult read_with_crlf(int fd);                                // Read message with CRLF marking the end of the message
    ReadResult read_with_size(int fd);                                // Read message with 4-byte size vector prepended to start of message

    // host number <-> network vector conversion
    std::vector<uint8_t> host_num_to_network_vector(uint32_t num);
    uint32_t network_vector_to_host_num(std::vector<char> &num_vec);
}

#endif