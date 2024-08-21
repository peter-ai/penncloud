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

#include <fcntl.h>
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
#include <shared_mutex>
#include <poll.h>
#include <random>
#include <algorithm>

#include "../../utils/include/utils.h"
#include "../../front_end/utils/include/fe_utils.h"

extern std::unordered_map<pthread_t, int> THREADS; // track threads
extern std::mutex MAP_MUTEX;                       // mutex for map of threads
extern int MAX_CONNECTIONS;                        // max pending connections that dispatcher can queue
extern int MAX_REQUEST;                            // max size of a request by coordinator
extern Logger logger;                              // setup logger
extern int SHUTDOWN;                               // shutdown flag
extern int VERBOSE;                                // verbose flag

/// @brief signal handler
/// @param sig signal to handle
void signal_handler(int sig);

/// @brief work to be done by thread servicing a request from a KVS server
/// @param arg a void pointer
/// @return void
void *kvs_thread(void *arg);

/// @brief work to be done by thread servicing a request from a front-end server
/// @param arg a void pointer
/// @return void
void *client_thread(void *arg);

/// @brief randomly sample an index between [0, length)
/// @param length the upper bound of the sampling range (exclusive)
/// @return a random index in range [0, length)
size_t sample_index(size_t length);

/// @brief constructs a specialized message for the given kvs
/// @param kvs an address of a kvs_args struct
/// @return a message to be sent back to the kvs
std::string get_kvs_message(struct kvs_args &kvs);

/// @brief broadcasts a specialized message to each member of the cluster specified by group number
/// @param group the cluster number of the calling kvs
void broadcast_to_cluster(int group);

/// @brief constructs message to be sent to admin HTTP server
/// @return a specialized message to admin
std::string get_admin_message();

/// @brief function to construct and send init message
/// @param kvs - kvs to send init to
/// @param request - request message
/// @return true is successful, false otherwise
bool send_kvs_init(struct kvs_args &kvs, std::string &request);

/// @brief function to construct and send reco message
/// @param kvs - kvs to send reco to
/// @param request - request message
/// @return true is successful, false otherwise
bool send_kvs_reco(struct kvs_args &kvs);

/// @brief client server connection
struct client_args
{
    std::string addr;    // address of client
    int fd;              // file descriptor for coordinator-http communication
    std::string request; // data read from client socket
};

/// @brief kvs server connection
struct kvs_args
{
    std::string client_addr; // binding address for http connections for the kvs
    std::string server_addr; // binding address for intra-kvs server communication
    std::string admin_addr;  // binding address for connections from admin console
    std::string kv_range;    // key value range kvs is responsible for
    bool primary;            // primary or not
    bool alive;              // alive or not
    int kvs_group;           // kvs cluser number
    int fd = -1;             // file descriptor for coordinator-kvs communication
};

#endif