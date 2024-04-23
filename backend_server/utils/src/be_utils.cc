#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>

#include "../include/be_utils.h"
#include "../../utils/include/utils.h"
#include "../../include/backend_server.h"

Logger be_utils_logger = Logger("BE Utils");

// @brief Open a connection to the specified port. Returns a fd if successful.
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

/**
 * WRITE OPERATIONS
 */

int BeUtils::write_to_coord(int fd, std::string &msg)
{
    // append delimiter to end of coordinator msg
    msg += "\r\n";
    // send msg to coordinator
    int bytes_sent = send(fd, (char *)msg.c_str(), msg.length(), 0);
    while (bytes_sent != msg.length())
    {
        if (bytes_sent < 0)
        {
            be_utils_logger.log("Unable to send message to coordinator", 40);
            return -1;
        }
        bytes_sent += send(fd, (char *)msg.c_str(), msg.length(), 0);
    }
    return 0;
}

int BeUtils::write(int fd, std::vector<char> &msg)
{
    // Insert size of message at beginning of msg
    std::vector<uint8_t> size_prefix = host_num_to_network_vector(msg.size());
    msg.insert(msg.begin(), size_prefix.begin(), size_prefix.end());

    // write response to provided fd until all bytes in vector are sent
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < msg.size())
    {
        int bytes_sent = send(fd, msg.data() + total_bytes_sent, msg.size() - total_bytes_sent, 0);
        if (bytes_sent < 0)
        {
            be_utils_logger.log("Unable to send message", 40);
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }
    return 0;
}

/**
 * READ OPERATIONS
 */

// ! might need to rework this to send back a ReadResult too
std::string BeUtils::read_from_coord(int fd)
{
    // read from fd
    std::string response;
    int bytes_recvd;
    bool res_complete = false;
    while (true)
    {
        char buf[1024]; // size of buffer for CURRENT read
        bytes_recvd = recv(fd, buf, sizeof(buf), 0);

        // error while reading from coordinator
        if (bytes_recvd < 0)
        {
            be_utils_logger.log("Unable to receive message from coordinator", 40);
            break;
        }
        // check condition where connection was preemptively closed by coordinator
        else if (bytes_recvd == 0)
        {
            be_utils_logger.log("Connection to coordinator closed", 50);
            break;
        }

        for (int i = 0; i < bytes_recvd; i++)
        {
            // check last index of coordinator's response for \r and curr index in buf for \n
            if (response.length() > 0 && response.back() == '\r' && buf[i] == '\n')
            {
                response.pop_back(); // delete \r in client message
                res_complete = true; // exit loop, we have full response from coordinator
            }
            response += buf[i];
        }
    }

    // only continue past this point if the response was complete (\r\n found at end of stream)
    if (!res_complete)
    {
        return "";
    }
    return response;
}

// @brief Read byte stream from fd
BeUtils::ReadResult BeUtils::read(int fd)
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

// @brief Read byte stream from fd with timeout for read
BeUtils::ReadResult BeUtils::read_with_timeout(int fd, int timeout_s)
{
    // initialize fd set to monitor for reads
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    // set timeout duration
    struct timeval timeout;
    timeout.tv_sec = timeout_s;
    timeout.tv_usec = 0;

    BeUtils::ReadResult read_result;

    // call select with specified timeout
    int result = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
    // error or timeout occurred during select call
    if (result <= 0)
    {
        read_result.error_code = -1;
        return read_result;
    }

    // data in fd - call read utility method to read from fd
    read_result = read(fd);
    return read_result;
}

std::vector<uint8_t> BeUtils::host_num_to_network_vector(uint32_t num)
{
    // convert to network order and interpret msg_size as bytes
    uint32_t num_in_network_order = htonl(num);
    std::vector<uint8_t> num_vec(sizeof(uint32_t));
    // Copy bytes from num_in_network_order into the size_prefix vector
    std::memcpy(num_vec.data(), &num_in_network_order, sizeof(uint32_t));
    return num_vec;
}

uint32_t BeUtils::network_vector_to_host_num(std::vector<char> &num_vec)
{
    // parse size from first 4 bytes of num vector
    uint32_t num;
    std::memcpy(&num, num_vec.data(), sizeof(uint32_t));
    // convert number received from network order to host order
    return ntohl(num);
}