#include <iostream>       
#include <unistd.h>       // getopt
#include <sys/socket.h>   // socket
#include <netinet/in.h>   // sockaddr_in

#include "../../utils/include/utils.h"

Logger kvs_logger("Storage server");

// main function to run storage server
// main expects the following flags:
// p - sets storage server's port number
// s - sets start of tablet range managed by server
// e - sets end of tablet range managed by server
// Example: backend_main -p 5000 -s aa -e zz
int main(int argc, char *argv[])
{
    int port;
    std::string range_start;
    std::string range_end;

    kvs_logger.log("Storage server running", 20);

    int opt;
    while ((opt = getopt(argc, argv, "p:s:e:")) != -1) {
        switch (opt) {
        case 'p':
            try {
                // set storage server's port
                port = std::stoi(optarg);
            } catch (std::invalid_argument const& ex) {
                kvs_logger.log("Invalid port number - exiting.", 40);
            }
            break;
        case 's':
            // assign start of tablet range managed by server
            range_start = optarg;
            if (range_start.length() != 2) {
                kvs_logger.log("Invalid start range - exiting", 40);
                return(-1);
            }
            break;
        case 'e':
            // assign end of tablet range managed by server
            range_end = optarg;
            if (range_end.length() != 2) {
                kvs_logger.log("Invalid end range - exiting", 40);
                return(-1);
            }
            break;
        case '?':
            break;
        }
    }

    kvs_logger = Logger("KVS Server " + range_start + ":" + range_end);

    kvs_logger.log("Storage server listening for connections on port " + std::to_string(port), 20);
    kvs_logger.log("Managing key range " + range_start + ":" + range_end, 20);
    return 0;
}