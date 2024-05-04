#include "../../utils/include/utils.h"
#include "../include/backend_server.h"

// main function to run storage server
// main expects the following flags:
// c - sets storage server's client listening port
// t - sets number of static tablets on this server
// Example: backend_main -c 6000 -t 5
int main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "c:t:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            try
            {
                // set storage server's client port
                BackendServer::client_port = std::stoi(optarg);
                BackendServer::group_port = BackendServer::client_port + 3000;
                BackendServer::admin_port = BackendServer::client_port + 6000;
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