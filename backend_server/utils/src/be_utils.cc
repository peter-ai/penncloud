#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fstream>

#include "../include/be_utils.h"
#include "../../utils/include/utils.h"
#include "../../include/backend_server.h"

Logger be_utils_logger = Logger("BE Utils");

// *********************************************
// CONNECTION METHODS
// *********************************************

/// @brief Creates a socket and binds the server to listen on the specified port. Returns a fd if successful, -1 otherwise.
int BeUtils::bind_socket(int port)
{
    // create server socket
    int sock_fd;
    if ((sock_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        be_utils_logger.log("Unable to create server socket.", 40);
        return -1;
    }

    // bind server socket to provided port (ensure port can be reused)
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if ((setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0)
    {
        be_utils_logger.log("Unable to reuse port to bind socket.", 40);
        return -1;
    }

    if ((bind(sock_fd, (const sockaddr *)&server_addr, sizeof(server_addr))) < 0)
    {
        be_utils_logger.log("Unable to bind socket to port.", 40);
        return -1;
    }

    // listen for connections on port
    const int BACKLOG = 20;
    if ((listen(sock_fd, BACKLOG)) < 0)
    {
        be_utils_logger.log("Unable to listen for connections on bound socket.", 40);
        return -1;
    }

    be_utils_logger.log("Server successfully bound on port " + std::to_string(port), 20);
    return sock_fd;
}

/// @brief Open a connection with the specified port. Returns a fd if successful, -1 otherwise.
int BeUtils::open_connection(int port)
{
    be_utils_logger.log("Opening connection with port " + std::to_string(port), 20);

    // Create socket to speak to port
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        be_utils_logger.log("Unable to create socket for communication", 40);
        return -1;
    }

    // set up address struct for destination
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::string ip = "127.0.0.1";
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1)
    {
        be_utils_logger.log("Invalid remote IP address", 40);
        return -1;
    }

    // Connect to port
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        be_utils_logger.log("Unable to establish connection", 40);
        return -1;
    }
    return sock_fd;
}

// *********************************************
// WRITE METHODS
// *********************************************

/// @brief Write message to fd, with crlf added to end of message
// Note that we always write with a copy of the message, since we're modifying the message here
int BeUtils::write_with_crlf(int fd, std::string msg)
{
    // append delimiter to end of msg
    msg += "\r\n";
    // send msg to fd
    int bytes_sent = send(fd, (char *)msg.c_str(), msg.length(), 0);
    while (bytes_sent != msg.length())
    {
        if (bytes_sent < 0)
        {
            be_utils_logger.log("Unable to send message", 40);
            return -1;
        }
        bytes_sent += send(fd, (char *)msg.c_str(), msg.length(), 0);
    }
    return 0;
}

/// @brief Write message to fd, with message size (4 bytes) prepended to start of message
// Note that we always write with a copy of the message, since we're modifying the message here
int BeUtils::write_with_size(int fd, std::vector<char> msg)
{
    // Insert size of message at beginning of msg
    std::vector<uint8_t> size_prefix = host_num_to_network_vector(msg.size());
    msg.insert(msg.begin(), size_prefix.begin(), size_prefix.end());

    // write msg to provided fd until all bytes in vector are sent
    size_t total_bytes_sent = 0;
    int num_retries = 3;
    while (total_bytes_sent < msg.size())
    {
        int bytes_sent = send(fd, msg.data() + total_bytes_sent, msg.size() - total_bytes_sent, 0);
        if (bytes_sent < 0)
        {
            // retry sending the message
            if (num_retries > 0)
            {
                num_retries--;
                continue;
            }

            // reached max number of retries for send, issue is likely with remote server
            be_utils_logger.log("Unable to send message", 40);
            return -1;
        }
        num_retries = 3; // reset number of retries on each successful send
        total_bytes_sent += bytes_sent;
    }
    return 0;
}

// *********************************************
// READ METHODS
// *********************************************

/// @brief Waits for READ events on a set of fds. Returns 0 if all fds responded. Returns -1 for error or timeout.
int BeUtils::wait_for_events(const std::vector<int> &fds, int timeout_ms)
{
    // create poll vector containing all input fds
    std::vector<pollfd> pollfds;
    for (int fd : fds)
    {
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN; // Waiting for input event to fd
        pollfds.push_back(pfd);
    }

    // set start time for polling and time left until polling expires
    auto start = std::chrono::system_clock::now();
    auto end = start + std::chrono::milliseconds(timeout_ms);
    int remaining_ms = timeout_ms;

    // loop until event is recorded on each fd or timeout occurs
    while (true)
    {
        int result = poll(pollfds.data(), pollfds.size(), remaining_ms);
        // error occurred
        if (result == -1)
        {
            be_utils_logger.log("Error occurred while polling", 40);
            return -1;
        }
        // timeout occurred
        else if (result == 0)
        {
            // be_utils_logger.log("Timeout occurred while polling", 20);
            return -1;
        }
        else
        {
            // check if an event was recorded on all fds
            bool all_fds_responded = true;
            for (const auto &pfd : pollfds)
            {
                // read event did not occur on this file descriptor - need to continue reading
                if (!(pfd.revents & POLLIN))
                {
                    all_fds_responded = false;
                    break;
                }
            }
            // read event occurred on all fds, return 0
            if (all_fds_responded)
            {
                be_utils_logger.log("Read event available on all fds", 20);
                return 0;
            }

            // Update remaining timeout
            auto now = std::chrono::system_clock::now();
            remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count();
            // no timeout left, return
            if (remaining_ms <= 0)
            {
                be_utils_logger.log("Timeout occurred while polling", 20);
                return -1;
            }
        }
    }
}

/// @brief Read message from fd, with CRLF marking the end of the message
BeUtils::ReadResult BeUtils::read_with_crlf(int fd)
{
    // struct to return from method
    BeUtils::ReadResult result;

    // read from fd
    int bytes_recvd;
    while (true)
    {
        char buf[1024]; // size of buffer for CURRENT read
        bytes_recvd = recv(fd, buf, sizeof(buf), 0);

        // error while reading from source
        if (bytes_recvd < 0)
        {
            be_utils_logger.log("Error reading from source", 40);
            result.error_code = -1;
            break;
        }
        // check condition where connection was preemptively closed by source
        else if (bytes_recvd == 0)
        {
            be_utils_logger.log("Remote socket closed connection", 40);
            result.error_code = -1;
            break;
        }

        for (int i = 0; i < bytes_recvd; i++)
        {
            // check last index of coordinator's response for \r and curr index in buf for \n
            if (result.byte_stream.size() > 0 && result.byte_stream.back() == '\r' && buf[i] == '\n')
            {
                result.byte_stream.pop_back(); // delete \r in client message
                return result;
            }
            result.byte_stream.push_back(buf[i]);
        }
    }
    return result;
}

/// @brief Read message from fd, with message size (4 bytes) prepended to start of message
BeUtils::ReadResult BeUtils::read_with_size(int fd)
{
    // struct to return from method
    BeUtils::ReadResult result;

    std::vector<char> byte_stream;
    uint32_t bytes_left = 0;

    int bytes_recvd;
    while (true)
    {
        char buf[4096];
        bytes_recvd = recv(fd, buf, 4096, 0);
        if (bytes_recvd < 0)
        {
            be_utils_logger.log("Error reading from source", 40);
            result.error_code = -1;
            break;
        }
        else if (bytes_recvd == 0)
        {
            be_utils_logger.log("Remote socket closed connection", 40);
            result.error_code = -1;
            break;
        }

        // iterate stream read from client and parse out message
        for (int i = 0; i < bytes_recvd; i++)
        {
            // message is not complete, append byte to byte_stream and decrement number of bytes left to read to complete message
            if (bytes_left != 0)
            {
                byte_stream.push_back(buf[i]);
                bytes_left--;

                // no bytes left in message - stop reading from fd and return byte stream read from fd
                if (bytes_left == 0)
                {
                    result.byte_stream = byte_stream;
                    return result;
                }
            }
            // command is complete, we need the next 4 bytes to determine how much bytes are in this command
            else
            {
                // now we have two situations
                // 1) client stream's size < 4, in which case the bytes left in the command is 0 only because the prev command was completed
                // 2) client stream's size >= 4
                if (byte_stream.size() < 4)
                {
                    byte_stream.push_back(buf[i]);
                    // parse size of command once client stream is 4 bytes long and store it in bytes_left
                    if (byte_stream.size() == 4)
                    {
                        // parse size of command from client
                        bytes_left = network_vector_to_host_num(byte_stream);
                        // clear the client stream in preparation for the incoming data
                        byte_stream.clear();
                    }
                }
            }
        }
    }
    return result;
}

// *********************************************
// HOST NUMBER <-> NETWORK VECTOR CONVERSION
// *********************************************

/// @brief Converts number in host order to number in network byte order, returning result as byte stream
std::vector<uint8_t> BeUtils::host_num_to_network_vector(uint32_t num)
{
    // convert to network order and interpret msg_size as bytes
    uint32_t num_in_network_order = htonl(num);
    std::vector<uint8_t> num_vec(sizeof(uint32_t));
    // Copy bytes from num_in_network_order into the size_prefix vector
    std::memcpy(num_vec.data(), &num_in_network_order, sizeof(uint32_t));
    return num_vec;
}

/// @brief Converts number in network byte order to host order, returning result as unsigned integer
uint32_t BeUtils::network_vector_to_host_num(std::vector<char> &num_vec)
{
    // parse size from first 4 bytes of num vector
    uint32_t num;
    std::memcpy(&num, num_vec.data(), sizeof(uint32_t));
    // convert number received from network order to host order
    return ntohl(num);
}

// *********************************************
// READ FROM FILE INTO VECTOR
// *********************************************

std::vector<char> BeUtils::read_from_file_into_vec(std::string &filename)
{
    // open file for reading
    std::ifstream file(filename, std::ios::binary);

    // get file's size
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the file into a vector
    std::vector<char> file_data(file_size);
    file.read(file_data.data(), file_size);

    // Close the file
    file.close();

    return file_data;
}