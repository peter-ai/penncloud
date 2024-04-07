#include <iostream>
#include <sys/socket.h>   // recv
#include <unistd.h>       // close
#include <sstream>        // istringstream

#include "client.h"
#include "http_request.h"
#include "utils.h"

const std::string Client::DOUBLE_CRLF = "\r\n\r\n";

void Client::read_from_network()
{
    // build request from client
    std::string client_stream;
    int bytes_recvd;

    while (true) {
        // TODO check if std::array is okay over using char[]
        std::array<char, 4096> buf;
        bytes_recvd = recv(m_client_fd, buf.data(), buf.size(), 0);

        // // error while reading from client
        // if (bytes_recvd < 0) {
        //     error("Error receiving data from client");
        //     // TODO should we break here?
        //     break;
        // }
        // // TODO I think this means the client closed the connection
        //     else if (bytes_recvd == 0) {
        //     break;
        // }

        // ! STRUCTURE
        // look for double CRLF (\r\n\r\n)
        // when you see this, parse the text and create an http request object

        for (int i = 0 ; i < bytes_recvd ; i++) {
            client_stream += buf[i];
            // double CRLF at end of client stream indicates a full http request
            if (client_stream.length() >= 4 && client_stream.substr(client_stream.length() - 4) == DOUBLE_CRLF) {
                // build http request from client stream and then clear stream
                HttpRequest req = parse_stream_for_req(client_stream);
                client_stream.clear();
            }
        }
    }
    close(m_client_fd);
}

HttpRequest parse_stream_for_req(std::string& client_stream) {
    HttpRequest req;
    std::string delimiter = "\r\n";
    std::vector<std::string> msg_lines = Utils::parse_string(client_stream, delimiter);

    // first line is always the request line
    std::vector<std::string> req_line = Utils::parse_string(msg_lines.at(0), " ");
    // verify req_line has 3 values
    if (req_line.size() != 3) {
        // ! write error message
    }
    req.req_method = req_line.at(0);
    req.path = req_line.at(1);
    req.version = req_line.at(2);

    // all remaining lines in msg_lines header lines
    for (auto it = msg_lines.begin() + 1; it != msg_lines.end(); it++) {
        // verify string is of non-zero length before parsing header
        if ((*it).length() != 0) {

        }
    }
    
    return req;
}