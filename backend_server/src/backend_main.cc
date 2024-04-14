#include <iostream>       
#include <unistd.h>       // getopt

#include "../../utils/include/utils.h"
#include "backend_server.h"

Logger kvs_logger("Backend server");

// main function to run storage server
// main expects the following flags:
// p - sets storage server's port number
// s - sets start of tablet range managed by server
// e - sets end of tablet range managed by server
// Example: backend_main -p 5000 -s aa -e zz
int main(int argc, char *argv[])
{
    kvs_logger.log("Storage server running", 20);

    int opt;
    while ((opt = getopt(argc, argv, "p:s:e:")) != -1) {
        switch (opt) {
        case 'p':
            try {
                // set storage server's port
                BackendServer::port = std::stoi(optarg);
            } catch (std::invalid_argument const& ex) {
                kvs_logger.log("Invalid port number - exiting.", 40);
            }
            break;
        case 's':
            // assign start of tablet range managed by server
            BackendServer::range_start = optarg;
            if (BackendServer::range_start.length() != 2) {
                kvs_logger.log("Invalid start range - exiting", 40);
                return(-1);
            }
            break;
        case 'e':
            // assign end of tablet range managed by server
            BackendServer::range_end = optarg;
            if (BackendServer::range_end.length() != 2) {
                kvs_logger.log("Invalid end range - exiting", 40);
                return(-1);
            }
            break;
        case '?':
            break;
        }
    }

    // update logger to display start and end range managed by server
    kvs_logger = Logger("Backend Server " + BackendServer::range_start + ":" + BackendServer::range_end);
    // Log metadata about backend server
    kvs_logger.log("Backend server listening for connections on port " + std::to_string(BackendServer::port), 20);
    kvs_logger.log("Managing key range " + BackendServer::range_start + ":" + BackendServer::range_end, 20);
    return 0;
}