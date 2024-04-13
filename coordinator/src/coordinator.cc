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

int main(int argc, char *argv[])
{
    Logger logger("Coordinator");
    int verbose = 0;

    /* -------------------- SERVER CL ARGUMENTS -------------------- */
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
            verbose = 1;
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
                        logger.log("Option '-s' requires a positive integer argument, coordinator exiting", LOGGER_ERROR);
                        return 1;
                    }
                }

                // if positive integer set port number
                kvs_servers = std::stoi(input);
                if (kvs_servers == 0)
                {
                    logger.log("Number of KVS server groups must be at least 1, " + std::to_string(kvs_servers) + " provided.", LOGGER_ERROR);
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
                        logger.log("Option '-b' requires a positive integer argument, coordinator exiting", LOGGER_ERROR);
                        return 1;
                    }
                }

                // if positive integer set number of backups per group
                kvs_backups = std::stoi(input);
                if (kvs_backups == 0)
                {
                    logger.log("Number of KVS backups per server group must be at least 1, " + std::to_string(kvs_backups) + " provided.", LOGGER_ERROR);
                    return 1;
                }

                break;
            }
        case '?':
            // handle unknown options characters
            if (isprint(optopt))
            {
                // if options characters are printable output the char
                logger.log("Unknown option character '-" + std::to_string(optopt) + "' is invalid, please provide a valid option", LOGGER_ERROR);
            }
            else
            {
                // if options characters are not printable, output hexcode
                logger.log("Unknown option character '-\\x" + std::to_string(optopt) + "' is invalid, please provide a valid option", LOGGER_ERROR);
            }
            return 1;
        default:
            // if error occurs, output message and error
            logger.log("Unable to parse command line arguments, server shutting down", LOGGER_ERROR);
            return 1;
        }
    }

    if (verbose)
    {
        logger.log("Server Groups: " + std::to_string(kvs_servers), LOGGER_INFO);
        logger.log("Backups/Server Group: " + std::to_string(kvs_backups), LOGGER_INFO);
    }

    // coordinator kvs data structures
    /*
        TODO: THESE WILL NEED MUTEXES ONCE WE SUPPORT RESTORES AND PRIMARY REALLOCATION
            SINCE THEY WILL NEED TO BE UPDATED DURING REASSIGNMENT OF PRIMARIES
    */
    std::unordered_map<char, std::unordered_map<std::string, std::string>> kvs_responsibilities;              // tracks the primary and secondaries for all letters
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> server_groups; // tracks the list of secondaries and the keys for each primary
    std::unordered_map<std::string, bool> kvs_health;

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
    if (verbose)
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

    

    return 0;
}