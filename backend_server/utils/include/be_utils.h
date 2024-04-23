#ifndef BE_UTILS_H
#define BE_UTILS_H

#include <string>
#include <vector>

namespace BeUtils
{
    // Open a connection to the specified port. Returns a fd if successful.
    int open_connection(int port);

    /**
     * Write operations
     */
    int write_to_coord(int fd, std::string &msg);    // Write message to coordinator
    int write(const int fd, std::vector<char> &msg); // Write message prepended with size prefix to fd

    /**
     * Read operations
     */
    // Define a struct to hold both the byte stream and an error code (for read and read_with_timeout)
    struct ReadResult
    {
        std::vector<char> byte_stream;
        int error_code = 0; // default error code of 0 (no error), -1 otherwise
    };
    std::string read_from_coord(int fd); // Read message from coordinator
    ReadResult read(int fd);             // Read byte stream from fd

    /**
     * Polling
     */

    int wait_for_events(std::vector<int> &fds, int timeout_ms); // Waits for event to occur on all fds within specified timeout

    // host number <-> network vector conversion
    std::vector<uint8_t> host_num_to_network_vector(uint32_t num);
    uint32_t network_vector_to_host_num(std::vector<char> &num_vec);
}

#endif