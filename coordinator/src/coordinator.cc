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
 *  !!! Port pattern for KVS servers is: 6[<0-index server_group#>][<0-indexed server#>]0
 *  !!! E.g.: 127.0.0.1:6000 is the address of the first server in the first server group
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

#include "../include/coordinator.h"

std::unordered_map<pthread_t, int> THREADS; // track threads
Logger logger("Coordinator");               // setup logger
int MAX_CONNECTIONS = 100;                  // max pending connections that dispatcher can queue
int MAX_REQUEST = 10000;                    // max size of a request by coordinator
std::mutex MAP_MUTEX;                       // mutex for map of threads
int SHUTDOWN = 0;                           // shutdown flag
int VERBOSE = 0;                            // verbose flag

std::unordered_map<std::string, struct kvs_args> kvs_intranet; // tracks which kvs intranet ip is associated with each kvs server
std::shared_timed_mutex intranet_mutex;
std::unordered_map<int, std::vector<struct kvs_args>> kvs_clusters; // tracks which kvs servers are currently a part of a kvs_cluster (must be alive)
std::shared_timed_mutex cluster_mutex;
std::unordered_map<char, std::vector<struct kvs_args>> client_map; // tracks the kvs servers responsible for each key
std::shared_timed_mutex client_map_mutex;

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
                for (unsigned long i = 0; i < input.length(); i++)
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
                for (unsigned long i = 0; i < input.length(); i++)
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

    float division = 26.0 / (float)kvs_servers;
    for (int groups = 0; groups < kvs_servers; groups++)
    {
        int lower = ceil((float)(groups)*division);
        int upper = (groups + 1 != kvs_servers ? ceil((float)(groups + 1) * division) : 26) - lower;

        std::vector<struct kvs_args> kvs_cluster;
        for (int servers = 0; servers <= kvs_backups; servers++)
        {
            std::string client_ip = ip_addr + ":6" + std::to_string(groups) + std::to_string(servers) + "0";
            std::string server_ip = ip_addr + ":9" + std::to_string(groups) + std::to_string(servers) + "0";
            std::string admin_ip = ip_addr + ":12" + std::to_string(groups) + std::to_string(servers) + "0";

            struct kvs_args kv;
            kv.alive = false;                           // server is not alive yet
            kv.kvs_group = groups;                      // set the group number of this server
            kv.primary = (servers == 0 ? true : false); // if server is 0 then make it primary, otherwise secondary
            kv.client_addr = client_ip;                 // set the client-facing ip:port address
            kv.server_addr = server_ip;                 // set the internal ip:port address
            kv.admin_addr = admin_ip;
            kv.kv_range = letters.substr(lower, upper); // set the key value range for the given kvs server

            // add kvs to client map
            for (size_t k = 0; k < kv.kv_range.size(); k++)
            {
                if (client_map.count(kv.kv_range[k]))
                    client_map[kv.kv_range[k]].push_back(kv);
                else
                    client_map[kv.kv_range[k]] = std::vector<struct kvs_args>({kv});
            }

            // store kvs in the intranet storage locator
            kvs_intranet[kv.server_addr] = kv;

            // add kvs to server group
            kvs_cluster.push_back(kv);
        }
        // add kvs to server group
        kvs_clusters[groups] = kvs_cluster;
    }

    if (VERBOSE)
    {
        logger.log("Client Map", LOGGER_DEBUG);
        for (auto &k : client_map)
        {
            std::string msg(1, k.first);
            msg += " <";
            for (size_t i = 0; i < k.second.size(); i++)
            {
                msg += (k.second[i].primary ? std::string("P-") : std::string("S-")) + k.second[i].client_addr + "/" + k.second[i].server_addr + ", ";
            }
            msg.pop_back();
            msg.pop_back();
            msg += ">";
            logger.log(msg, LOGGER_DEBUG);
        }
    }
    logger.log("KVS server responsibilites set", LOGGER_INFO);

    
    /* -------------------- SEND ADMIN MESSAGE -------------------- */
    // get details and open socket for comms
    int admin_port = 8080;
    int admin_sock = FeUtils::open_socket(ip_addr, admin_port);

    // construct message
    std::string admin_msg = get_admin_message();    

    // send message
    int sent = 0;
    if ((sent = send(admin_sock, &admin_msg[0], admin_msg.size(), 0)) == -1)
    {
        logger.log("Failed to send data (" + std::string(strerror(errno)) + ")", LOGGER_ERROR);
    }
    else
    {
        if (VERBOSE)
            logger.log("Sent message to <Admin>: " + admin_msg, LOGGER_INFO);
    }
    
    // close socket
    close(admin_sock);

    /* -------------------- DISPATCHER SETUP -------------------- */
    // create streaming socket
    int dispatcher_fd = socket(PF_INET, SOCK_STREAM, 0);
    THREADS[pthread_self()] = dispatcher_fd;
    int port = 4999;

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
            // extract source ip address and port (important - convert port from network order to host order)
            std::string source = std::string(inet_ntoa(src.sin_addr)) + ":" + std::to_string(ntohs(src.sin_port));
            intranet_mutex.lock_shared();
            int present = kvs_intranet.count(source);
            intranet_mutex.unlock_shared();

            // if connection is from a kvs
            if (present)
            {
                // get kvs associated with this connection
                intranet_mutex.lock_shared();
                struct kvs_args kvs = kvs_intranet[source];
                intranet_mutex.unlock_shared();
                kvs.fd = comm_fd;

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
                if (pthread_create(&thid, NULL, client_thread, (void *)&client) != 0)
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

/// @brief signal handler
/// @param sig signal to handle
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

/// @brief work to be done by thread servicing a request from a KVS server
/// @param arg a void pointer
/// @return void
void *kvs_thread(void *arg)
{
    /* -------------------- WORKER SETUP -------------------- */
    // extract the client struct
    struct kvs_args *kvs = (struct kvs_args *)arg;
    kvs->alive = true;

    // setup polling for KVS server
    std::vector<struct pollfd> fds(1);
    fds[0].fd = kvs->fd;
    fds[0].events = POLLIN;   // awaiting POLLIN event
    int timeout_msecs = 4000; // timeout 4000 milliseconds = 4 seconds
    int ret;

    while ((ret = poll(fds.data(), fds.size(), timeout_msecs)) != -1)
    {
        if (ret > 0) // socket is ready to be read
        {
            // if events triggered, do stuff
            if (fds[0].revents & POLLIN)
            {
                // define string as buffer for requests
                std::string request;
                request.resize(MAX_REQUEST);
                int rlen = 0;
                int sent = 0;

                // read until CRLF is encountered
                while (request[rlen] != '\n' && request[rlen - 1] != '\r')
                {
                    // read from socket connection
                    rlen += recv(kvs->fd, &request[0], request.size() - rlen, 0);

                    // if an error occurs log it
                    if (rlen == -1)
                    {
                        logger.log("Failed to received data (" + std::string(strerror(errno)) + ")", LOGGER_ERROR);
                    }

                    if (rlen == 0) // socket closed
                    {
                        break;
                    }
                }
                if (rlen == 0)
                    break; // if rlen == 0, the socket was closed

                // resize request and extract command
                request.resize(rlen);
                std::string command = request.substr(0, 4);

                if (command.compare("INIT") == 0) // Scenario 1 - INIT
                {
                    // construct message
                    std::string response = (kvs->primary ? "P " : "S ") + get_kvs_message(*kvs);

                    // send response to kvs
                    if ((sent = send(kvs->fd, &response[0], response.size(), 0)) == -1)
                    {
                        logger.log("Failed to send data (" + std::string(strerror(errno)) + ")", LOGGER_CRITICAL);
                        if (errno == ECONNRESET)
                            break; // connection has been closed so exit loop
                    }
                    else
                    {
                        if (VERBOSE)
                            logger.log("Sent response to <" + kvs->server_addr + ">: " + response, LOGGER_INFO);
                    }

                    request.clear();
                    response.clear();
                }
                else if (command.compare("PING") == 0) // Scenario 2 - PING
                    kvs->alive = true;
                else // Scenario 3 - RECO
                {
                    if (kvs->alive)
                        continue; // if kvs is already alive, do not process the below commands

                    // kvs is alive
                    kvs->alive = true;

                    // add alive server to client map
                    client_map_mutex.lock();
                    for (auto &key : kvs->kv_range)
                    {
                        client_map[key].push_back(*kvs);
                    }
                    client_map_mutex.unlock();

                    // add alive server to cluster group
                    cluster_mutex.lock();
                    kvs_clusters[kvs->kvs_group].push_back(*kvs);
                    cluster_mutex.unlock();

                    // broadcast updated server list and primary to all kvs in cluster
                    broadcast_to_cluster(kvs->kvs_group);
                }
            }
        }
        else if (ret == -1) // failure occured
        {
            logger.log("Polling socket for KVS " + kvs->client_addr + "/" + kvs->server_addr + "failed. (" + strerror(errno) + ")", LOGGER_ERROR);
        }
        else // call timed out and no fds are ready to be read from
        {
            if (!kvs->alive)
                continue; // if kvs is already dead, do not process the below commands

            // kvs is dead
            kvs->alive = false;

            // remove dead server from client map
            client_map_mutex.lock();
            for (auto &key : kvs->kv_range)
            {
                std::vector<kvs_args>::iterator position = std::find(client_map[key].begin(), client_map[key].end(), kvs);
                if (position != client_map[key].end())
                    client_map[key].erase(position);
            }
            client_map_mutex.unlock();

            // remove dead server from cluster group
            cluster_mutex.lock();
            std::vector<kvs_args>::iterator position = std::find(kvs_clusters[kvs->kvs_group].begin(), kvs_clusters[kvs->kvs_group].end(), kvs);
            if (position != kvs_clusters[kvs->kvs_group].end())
                kvs_clusters[kvs->kvs_group].erase(position);

            // if current server was primary select a new primary
            if (kvs->primary)
            {
                // no longer primary
                kvs->primary = false;

                // assign new primary at random
                size_t candidate = sample_index(kvs_clusters[kvs->kvs_group].size());
                kvs_clusters[kvs->kvs_group][candidate].primary = true;
            }
            cluster_mutex.unlock();

            // broadcast updated server list and primary to all kvs in cluster
            broadcast_to_cluster(kvs->kvs_group);
        }
    }

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

/// @brief work to be done by thread servicing a request from a front-end server
/// @param arg a void pointer
/// @return void
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
    request.resize(rlen);

    if (VERBOSE)
    {
        if (VERBOSE)
            logger.log("Received request from <" + client->addr + ">: " + request, LOGGER_INFO);
    }

    if (rlen != -1)
    {
        // assign appropriate kvs for given request by randomly sampling vector
        char key = request[0];
        client_map_mutex.lock_shared();
        // std::string kvs_server = (client_map.count(key) ? client_map[key][sample_index(client_map[key].size())].client_addr : "-ERR First character non-alphabetical"); // TODO: UNCOMMENT THIS AND TEST
        std::string kvs_server = (client_map.count(request[0]) ? client_map[request[0]][0].client_addr : "-ERR First character non-alphabetical"); // just select first kvs in vector
        client_map_mutex.unlock_shared();
        logger.log("KVS choice for " + std::string(1, key) + " is " + kvs_server, LOGGER_INFO);

        // send response
        if ((sent = send(client->fd, &kvs_server[0], kvs_server.size(), 0)) == -1)
        {
            logger.log("Failed to send data (" + std::string(strerror(errno)) + ")", LOGGER_ERROR);
        }
        else
        {
            if (VERBOSE)
                logger.log("Sent response to <" + client->addr + ">: " + kvs_server, LOGGER_INFO);
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

/// @brief randomly sample an index between [0, length)
/// @param length the upper bound of the sampling range (exclusive)
/// @return a random index in range [0, length)
size_t sample_index(size_t length)
{
    std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<std::size_t> distribution(0, length - 1);
    return distribution(generator);
}

/// @brief constructs a specialized message for the given kvs
/// @param kvs an address of a kvs_args struct
/// @return a message to be sent back to the kvs
std::string get_kvs_message(kvs_args &kvs)
{
    // construct message
    std::string response = std::string(1, kvs.kv_range[0]) + ":" + std::string(1, kvs.kv_range.back()) + " "; // add key value range to message
    std::string secondaries = "";

    cluster_mutex.lock_shared();
    for (auto &server : kvs_clusters[kvs.kvs_group])
    {
        if (server.primary)
            response += server.server_addr + " "; // add primary to message
        else
            secondaries += server.server_addr + " "; // create list of secondaries
    }
    cluster_mutex.unlock_shared();

    secondaries.pop_back();           // remove final trailing whitespace from message
    response += secondaries + "\r\n"; // add secondaries to message with terminating CRLF

    return response;
}

/// @brief broadcasts a specialized message to each member of the cluster specified by group number
/// @param group the cluster number of the calling kvs
void broadcast_to_cluster(int group)
{
    cluster_mutex.lock_shared();
    std::string message = get_kvs_message(kvs_clusters[group][0]);
    for (auto &kvs : kvs_clusters[group])
    {
        std::string response = (kvs.primary ? "P " : "S ") + message;                                                       // construct message for this kvs
        int idx = kvs.server_addr.find(':');                                                                                // split address
        int kv_sock = FeUtils::open_socket(kvs.server_addr.substr(0, idx - 1), std::stoi(kvs.server_addr.substr(idx + 1))); // open socket for message

        int sent = 0;
        if ((sent = send(kvs.fd, &response[0], response.size(), 0)) == -1)
        {
            logger.log("Failed to send data (" + std::string(strerror(errno)) + ")", LOGGER_CRITICAL);
        }

        logger.log("Message broadcasted to <" + kvs.server_addr + ">", LOGGER_INFO);
        close(kv_sock);
    }
    cluster_mutex.unlock_shared();
}

/// @brief constructs message to be sent to admin HTTP server
/// @return a specialized message to admin
std::string get_admin_message()
{
    std::string message = "C ";

    cluster_mutex.lock_shared();
    for (auto &cluster: kvs_clusters)
    {
        // add group number
        message += "SG" + std::to_string(cluster.first) + ": ";
        
        // create list of servers for each cluster/group
        bool primary = false;
        std::string temp = "";
        for (size_t i = 0; i < cluster.second.size(); i++)
        {   
            kvs_args kv = cluster.second[i];

            // add name
            if (kv.primary) 
            {
                temp += "primary ";
                primary = true;
            }    
            else 
            {
                temp += "secondary" + (primary ? std::to_string(i) : std::to_string(i-1)) + " ";
            }

            // add port
            temp += kv.server_addr.substr(kv.server_addr.find(':')+1) + ",";
        }
        temp.pop_back();

        // add cluster of servers to message
        message += temp + "\n";
    }
    cluster_mutex.unlock_shared();
    
    message += "\r\n"; // add terminating characters
    return message;
}  
