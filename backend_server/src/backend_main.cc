#include "../../utils/include/utils.h"
#include "../include/backend_server.h"

// main function to run storage server
// main expects the following flags:
// p - sets storage server's port number
// t - sets number of static tablets on this server
// Example: backend_main -p 5000 -s aa -e zz
int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "c:g:t:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            try
            {
                // set storage server's client port
                BackendServer::client_port = std::stoi(optarg);
            }
            catch (std::invalid_argument const &ex)
            {
                return -1;
            }
            break;
        case 'g':
            try
            {
                // set storage server's group communication port
                BackendServer::group_port = std::stoi(optarg);
            }
            catch (std::invalid_argument const &ex)
            {
                return -1;
            }
            break;
        case 't':
            try
            {
                // set number of tablets in server
                BackendServer::num_tablets = std::stoi(optarg);
            }
            catch (std::invalid_argument const &ex)
            {
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