#include "../../utils/include/utils.h"
#include "../include/backend_server.h"

// main function to run storage server
// main expects the following flags:
// p - sets storage server's port number
// s - sets start of tablet range managed by server
// e - sets end of tablet range managed by server
// Example: backend_main -p 5000 -s aa -e zz
int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "c:p:s:e:t:")) != -1) {
        switch (opt) {
            case 'c':
                try {
                    // set coordinator server's port
                    BackendServer::coord_port = std::stoi(optarg);
                } catch (std::invalid_argument const& ex) {
                    return -1;
                }
                break;
            case 'p':
                try {
                    // set storage server's port
                    BackendServer::port = std::stoi(optarg);
                } catch (std::invalid_argument const& ex) {
                    return -1;
                }
                break;
            case 's':
                // assign start of tablet range managed by server
                BackendServer::range_start = optarg;
                if (BackendServer::range_start.length() != 1) {
                    return -1;
                }
                break;
            case 'e':
                // assign end of tablet range managed by server
                BackendServer::range_end = optarg;
                if (BackendServer::range_end.length() != 1) {
                    return -1;
                }
                break;
            case 't':
                try {
                    // set number of tablets in server
                    BackendServer::num_tablets = std::stoi(optarg);
                } catch (std::invalid_argument const& ex) {
                    return -1;
                }
                break;
            case '?':
                break;
        }
    }

    BackendServer::run();
    return 0;
}