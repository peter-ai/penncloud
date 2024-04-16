#ifndef COORDINATOR_H
#define COORDINATOR_H
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
 */

#include <iostream>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <strings.h>
#include <stdlib.h>

#include "../../utils/include/utils.h"

extern std::unordered_map<pthread_t, int> THREADS; // track threads
extern std::mutex MAP_MUTEX;                       // mutex for map of threads
extern int MAX_CONNECTIONS;                        // max pending connections that dispatcher can queue
extern int MAX_REQUEST;                            // max size of a request by coordinator
extern Logger logger;                              // setup logger
extern int SHUTDOWN;                               // shutdown flag
extern int VERBOSE;                                // verbose flag

extern std::unordered_map<char, std::unordered_map<std::string, std::string>> kvs_responsibilities;              // tracks the primary and secondaries for all letters
extern std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> server_groups; // tracks the list of secondaries and the keys for each primary
extern std::unordered_map<std::string, bool> kvs_health;                                                         // tracks which kvs servers are alive

void signal_handler(int sig);   // signal handler
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

#endif