/*
 * coordinator.cc
 *
 *  Created on: Apr 13, 2024
 *      Author: peter-ai
 *
 *  Coordinator handles incoming requests taking in a file path
 *  and locating the KVS server that is responsible for that file
 *  path according to the first letter of the path and the key-value
 *  range assigned to each KVS server
 *
 *  Coordinator stores ancillary data structures on launch to alleviate burden
 *  of primary reassignment later.
 *
 *  Launch: ./coordinator [-v verbose mode] [-s # number of server groups] [-b # number of backups per kvs group]
 *
 *  !!! Coordinator starts on 127.0.0.1:4999 !!!
 *  !!! Port pattern for KVS servers is: 5[<0-index server_group#>][<0-indexed server#>]0
 *  !!! E.g.: 127.0.0.1:5000 is the address of the first server in the first server group
 *
 *  1) kvs_responsibilities - unordered_map
 *      Data structure keeps track of, for each letter/char the ip:port
 *      for its current primary and its corresponding secondaries
 *      Example for 3 server groups with 2 backups per group
 *      {
 *          'a': {"primary": "127.0.0.1:5000", "secondary1": "127.0.0.1:5010", "secondary2": "127.0.0.1:5020",...},
 *          'b': {"primary": "127.0.0.1:5000", "secondary1": "127.0.0.1:5010", "secondary2": "127.0.0.1:5020",...},
 *          ...,
 *          'j': {"primary": "127.0.0.1:5100", "secondary1": "127.0.0.1:5110", "secondary2": "127.0.0.1:5120",...},
 *          ...,
 *          'z': {"primary": "127.0.0.1:5200", "secondary1": "127.0.0.1:5210", "secondary2": "127.0.0.1:5220",...},
 *      }
 *  2) server_groups - unordered_map
 *      Data structure keeps track of, for each server group,
 *      who the primary is and who the set of secondaries are
 *      and what is the key-value range that is associated with the group
 *      (stores inverse info of kvs_responsibilities)
 *      Example for 3 server groups with 2 backups per group
 *      {
 *          "127.0.0.1:5000": {"backups": <"127.0.0.1:5010", "127.0.0.1:5020">, "key_range": <"abcdefghi">},
 *          "127.0.0.1:5100": {"backups": <"127.0.0.1:5110", "127.0.0.1:5120">, "key_range": <"jklmnopqr">},
 *          "127.0.0.1:5200": {"backups": <"127.0.0.1:5210", "127.0.0.1:5220">, "key_range": <"stuvwxyz">},
 *      }
 *  3) kvs_health - unordered_map
 *      tracks the health (alive=true, dead=false) for every kvs server in the system
 *      Example for 3 server groups with 2 backups per group
 *      {
 *          "127.0.0.1:5000": true,
 *          "127.0.0.1:5010": true,
 *          "127.0.0.1:5020": false,
 *          ...,
 *          "127.0.0.1:5100": true,
 *          ...,
 *          "127.0.0.1:5200": true,
 *          "127.0.0.1:5210", true,
 *          "127.0.0.1:5220": true,
 *      }
 */

#include <iostream>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include "../../utils/include/utils.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <strings.h>
#include <stdlib.h>

Logger logger("Coordinator");               // setup logger
int SHUTDOWN = 0;                           // shutdown flag
std::unordered_map<pthread_t, int> THREADS; // track threads
std::mutex MAP_MUTEX;                       // mutex for map of threads
int MAX_CONNECTIONS = 100;                  // max pending connections that dispatcher can queue
int MAX_REQUEST = 10000;                    // max size of a request by coordinator
int VERBOSE = 0;

// coordinator kvs data structures
/*
    TODO: THESE WILL NEED MUTEXES ONCE WE SUPPORT RESTORES AND PRIMARY REALLOCATION
        SINCE THEY WILL NEED TO BE UPDATED DURING REASSIGNMENT OF PRIMARIES
*/
std::unordered_map<char, std::unordered_map<std::string, std::string>> kvs_responsibilities;              // tracks the primary and secondaries for all letters
std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> server_groups; // tracks the list of secondaries and the keys for each primary
std::unordered_map<std::string, bool> kvs_health;

void signal_handler(int sig);
void *kvs_thread(void *arg);    // work to be done by thread servicing a request from a KVS server
void *client_thread(void *arg); // work to be done by thread servicing a request from a front-end server

struct client_args
{
    std::string addr;
    int fd;
};

struct kvs_args
{
    std::string addr;
    int fd;
};

int main(int argc, char *argv[])
{
    /* -------------------- SERVER SIGNAL HANDLING -------------------- */
    struct sigaction server_sig;
    server_sig.sa_handler = signal_handler;
    server_sig.sa_flags = 0;
    sigaction(SIGINT, &server_sig, NULL);
    sigaction(SIGUSR1, &server_sig, NULL);

    /* -------------------- COMMAND LINE ARGUMENTS -------------------- */
    opterr = 0; // surpress default error output
    int option; // var for reading in command line arguments

    int kvs_servers = 3; // default 3 server groups
    int kvs_backups = 2; // default 2 backups per server group

    // read command line options
    while ((option = getopt(argc, argv, "vs:b:")) != -1)
    {
        switch (option)
        {
        case 'v':
            VERBOSE = 1;
            break;
        case 's':
            if (optarg)
            {
                // read command line argument
                std::string input(optarg);

                // check if option is positive integer
                for (int i = 0; i < input.length(); i++)
                {
                    if (!std::isdigit(input[i]))
                    {
                        logger.log("Option '-s' requires a positive integer argument, coordinator exiting", LOGGER_CRITICAL);
                        return 1;
                    }
                }

                // if positive integer set port number
                kvs_servers = std::stoi(input);
                if (kvs_servers == 0)
                {
                    logger.log("Number of KVS server groups must be at least 1, " + std::to_string(kvs_servers) + " provided.", LOGGER_CRITICAL);
                    return 1;
                }
                break;
            }
        case 'b':
            if (optarg)
            {
                // read command line argument
                std::string input(optarg);

                // check if option is positive integer
                for (int i = 0; i < input.length(); i++)
                {
                    if (!std::isdigit(input[i]))
                    {
                        logger.log("Option '-b' requires a positive integer argument, coordinator exiting", LOGGER_CRITICAL);
                        return 1;
                    }
                }

                // if positive integer set number of backups per group
                kvs_backups = std::stoi(input);
                if (kvs_backups == 0)
                {
                    logger.log("Number of KVS backups per server group must be at least 1, " + std::to_string(kvs_backups) + " provided.", LOGGER_CRITICAL);
                    return 1;
                }

                break;
            }
        case '?':
            // handle unknown options characters
            if (isprint(optopt))
            {
                // if options characters are printable output the char
                logger.log("Unknown option character '-" + std::to_string(optopt) + "' is invalid, please provide a valid option", LOGGER_CRITICAL);
            }
            else
            {
                // if options characters are not printable, output hexcode
                logger.log("Unknown option character '-\\x" + std::to_string(optopt) + "' is invalid, please provide a valid option", LOGGER_CRITICAL);
            }
            return 1;
        default:
            // if error occurs, output message and error
            logger.log("Unable to parse command line arguments, server shutting down", LOGGER_CRITICAL);
            return 1;
        }
    }

    if (VERBOSE)
    {
        logger.log("Server Groups: " + std::to_string(kvs_servers), LOGGER_INFO);
        logger.log("Backups/Server Group: " + std::to_string(kvs_backups), LOGGER_INFO);
    }

    /* -------------------- KVS SERVER INFORMATION -------------------- */
    std::string letters = "abcdefghijklmnopqrstuvwxyz";
    std::string ip_addr = "127.0.0.1";
    int server_group = 0;
    int i = 0;

    // set key value ranges for each kvs server group
    // store primary, secondaries and key value ranges for each server group
    // establish server health
    while (i < letters.size())
    {
        if (i < (((float)letters.length() / (float)kvs_servers) * (float)(server_group + 1)))
        {
            int server_number = 0;
            std::unordered_map<std::string, std::string> resp;
            std::vector<std::string> backups;
            std::string secondary_kvs;
            std::string primary_kvs = ip_addr + ":5" + std::to_string(server_group) + std::to_string(server_number) + "0";

            resp["primary"] = primary_kvs;

            server_number++;

            kvs_health[primary_kvs] = true;
            server_groups[primary_kvs]["key_range"].push_back(letters.substr(i, 1));

            while (server_number <= kvs_backups)
            {
                secondary_kvs = ip_addr + ":5" + std::to_string(server_group) + std::to_string(server_number) + "0";
                resp["secondary" + std::to_string(server_number)] = secondary_kvs;

                kvs_health[secondary_kvs] = true;
                backups.push_back(secondary_kvs);
                server_number++;
            }
            kvs_responsibilities[letters[i]] = resp;

            if (!server_groups[primary_kvs].count("backups"))
                server_groups[primary_kvs]["backups"] = backups;

            i++;
        }
        else
        {
            server_group++;
        }
    }

    // verbose printing
    if (VERBOSE)
    {
        for (auto &res : kvs_responsibilities)
        {
            std::string msg = std::string(1, res.first) + " ";
            for (auto &backups : res.second)
            {
                msg += backups.first + "=" + backups.second + " ";
            }
            logger.log(msg, LOGGER_INFO);
        }

        for (auto &group : server_groups)
        {
            std::string msg = group.first + " <";
            for (i = 0; i < group.second["backups"].size(); i++)
                msg += group.second["backups"][i] + (i != group.second["backups"].size() - 1 ? ", " : "");
            msg += "> <";
            for (i = 0; i < group.second["key_range"].size(); i++)
                msg += group.second["key_range"][i];
            msg += ">";
            logger.log(msg, LOGGER_INFO);
        }
    }
    logger.log("KVS server responsibilites set", LOGGER_INFO);

    /* -------------------- DISPATCHER SETUP -------------------- */
    // create streaming socket
    int dispatcher_fd = socket(PF_INET, SOCK_STREAM, 0);
    THREADS[pthread_self()] = dispatcher_fd;
    int port = 4999;

    // fcntl(listener_fd, F_SETFL, (fcntl(listener_fd, F_GETFL)|O_NONBLOCK));
    // THREADS[pthread_self()] = listener_fd;

    // setup socket details
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_aton(ip_addr.c_str(), &servaddr.sin_addr); // servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    // bind socket to port
    if (bind(dispatcher_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::string msg = "Cannot bind socket to port #" + std::to_string(port) + " (" + strerror(errno) + ")";
        logger.log(msg, LOGGER_CRITICAL);
        return 1;
    }

    // begin listening to incoming connections
    if (listen(dispatcher_fd, MAX_CONNECTIONS) == -1)
    {
        std::string msg = "Socket cannot listen";
        logger.log(msg, LOGGER_CRITICAL);
        return 1;
    }

    /* -------------------- COORDINATING -------------------- */
    pthread_t thid; // var for new thread_ids

    // setup address struct for message from client or from other servers
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);

    // begin accepting connections and processing incoming requests
    while (true)
    {
        if (SHUTDOWN)
            break;

        // accept incoming connections
        int comm_fd = accept(dispatcher_fd, (struct sockaddr *)&src, &srclen);

        // if valid connection created (no error on accept)
        if (comm_fd != -1)
        {
            // create new thread
            // int *thread_fd = (int *)malloc(sizeof(int));
            // *thread_fd = comm_fd;

            // extract source ip address and port (important - convert port from network order to host order)
            std::string source = std::string(inet_ntoa(src.sin_addr)) + ":" + std::to_string(ntohs(src.sin_port));

            // if connection is from a kvs
            if (kvs_health.count(source))
            {
                struct kvs_args kvs;
                if (pthread_create(&thid, NULL, kvs_thread, (void *)&kvs) != 0)
                {
                    logger.log("Error, unable to create new thread.", LOGGER_CRITICAL);
                    return 1;
                }
            }
            else
            {
                struct client_args client;
                client.addr = source;
                client.fd = comm_fd;

                // give thread relavent handler
                if (pthread_create(&thid, NULL, (kvs_health.count(source) ? kvs_thread : client_thread), (void *)&client) != 0)
                {
                    logger.log("Error, unable to create new thread.", LOGGER_CRITICAL);
                    return 1;
                }
            }

            // acquire mutex on threads map and store new thread id
            MAP_MUTEX.lock();
            THREADS[thid] = comm_fd;
            MAP_MUTEX.unlock();
        }
        if (SHUTDOWN)
            break;
    }

    /* -------------------- COORDINATOR SHUTDOWN -------------------- */
    // check if coordinator has received SIGINT
    if (SHUTDOWN == 1)
    {
        // join all threads once they've exited
        for (auto &thread : THREADS)
        {
            thid = thread.first;
            void *ret;

            pthread_join(thid, &ret);
        }
    }

    close(dispatcher_fd);
    return 0;
}

// function to handle the reception of SIGINT and SIGUSR1 signals
void signal_handler(int sig)
{
    // if thread received a SIGINT
    if (sig == SIGINT)
    {
        // set global shutdown flag to 1
        SHUTDOWN = 1;
        pthread_t calling_thread = pthread_self();

        // send a SIGUSR1 signal to all other threads
        for (auto &thread : THREADS)
        {
            pthread_t thid = thread.first;

            if (pthread_equal(thid, calling_thread) == 0)
            {
                pthread_kill(thid, SIGUSR1);
            }
        }
        return;
    }
    // if thread received a SIGUSR1 signal, return
    else
    {
        return;
    }
}

// TODO: IMPLEMENT
// work to be done by thread servicing a request from a KVS server
void *kvs_thread(void *arg)
{
    /* -------------------- WORKER SETUP -------------------- */
    // extract the client struct
    struct kvs_args *kvs = (struct kvs_args *)arg;

    // detach self - notify kernel to reclaim resources
    if (SHUTDOWN != 1)
        pthread_detach(pthread_self());


    // clean up thread resources
    MAP_MUTEX.lock();
    THREADS.erase(pthread_self());
    MAP_MUTEX.unlock();
    close(kvs->fd);

    // shutdown thread
    int *status = 0;
    pthread_exit((void *)status);
}

// work to be done by thread servicing a request from a front-end server
void *client_thread(void *arg)
{
    /* -------------------- WORKER SETUP -------------------- */
    // extract the client struct
    struct client_args *client = (struct client_args *)arg;

    // define string as buffer for requests
    std::string request;
    request.resize(MAX_REQUEST);
    int rlen = 0;
    int sent = 0;

    // receive incoming request
    if ((rlen = recv(client->fd, &request[0], request.size() - rlen, 0)) == -1)
    {
        logger.log("Failed to received data (" + std::string(strerror(errno)) + ")", LOGGER_ERROR);
    }
    request.resize(rlen-1);


    if (VERBOSE)
    {
        if (VERBOSE) logger.log("Received request from <"+ client->addr + ">: " + request, LOGGER_INFO);
    }

    if (rlen != -1)
    {
        // TODO: Revisit kvs server selection logic??
        // assign appropriate kvs for given request
        std::string kvs_server = (kvs_responsibilities.count(request[0]) ? kvs_responsibilities[request[0]]["primary"] : "-ERR First character non-alphabetical");
        
        // send response
        if ((sent = send(client->fd, &kvs_server[0], kvs_server.size(), 0)) == -1)
        {
            logger.log("Failed to send data (" + std::string(strerror(errno)) + ")", LOGGER_ERROR);
        }
        else
        {
            if (VERBOSE) logger.log("Sent response to <" + client->addr + ">: " + kvs_server, LOGGER_INFO);
        }
    }

    // clean up thread resources
    MAP_MUTEX.lock();
    THREADS.erase(pthread_self());
    MAP_MUTEX.unlock();
    close(client->fd);

    // detach self - notify kernel to reclaim resources
    if (SHUTDOWN != 1)
        pthread_detach(pthread_self());

    // shutdown thread
    int *status = 0;
    pthread_exit((void *)status);
}