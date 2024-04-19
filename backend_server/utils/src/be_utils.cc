#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>

#include "../include/be_utils.h"
#include "../../utils/include/utils.h"

// Initialize logger with info about start and end of key range
Logger be_utils_logger = Logger("BE Utils");

int BeUtils::open_connection(int port)
{
    // Create socket to speak to port
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
    {
        be_utils_logger.log("Unable to create socket for communication", 40);
        return -1;
    }

    // set up address struct
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::string address = "127.0.0.1:" + std::to_string(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    // Connect to port
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        be_utils_logger.log("Unable to establish connection", 40);
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}

int BeUtils::write(const int fd, const std::string &msg)
{
    // send msg to coordinator
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

std::string BeUtils::read(int fd)
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
            be_utils_logger.log("Unable to receive message", 40);
            break;
        }
        // check condition where connection was preemptively closed by coordinator
        else if (bytes_recvd == 0)
        {
            be_utils_logger.log("Connection closed", 40);
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