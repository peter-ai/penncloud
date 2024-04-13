/*
 * coordinator.cc
 *
 *  Created on: Apr 13, 2024
 *      Author: peter-ai
 */

#include <iostream>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include "../../utils/include/utils.h"

int main(int argc, char *argv[])
{
    Logger logger("Coordinator");

    /* -------------------- SERVER CL ARGUMENTS -------------------- */
    opterr = 0; // surpress default error output
    int option; // var for reading in command line arguments

    int kvs_servers = 3; // default 3 server groups
    int kvs_backups = 2; // default 2 backups per server group

    // read command line options
    while ((option = getopt(argc, argv, "s:b:")) != -1)
    {
        switch (option)
        {
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

    // coordinator kvs data structures
    std::unordered_map<char, std::unordered_map<std::string, std::string>> kvs_responsibilities;
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::string>>> server_groups;

    std::string letters = "abcdefghijklmnopqrstuvwxyz";
    std::string ip_addr = "127.0.0.1";
    int server_group = 0;
    int i = 0;

    while (i < letters.size())
    {
        if (i < (((float)letters.length() / (float)kvs_servers)*(float)(server_group+1)))
        {
            int server_number = 0;
            std::unordered_map<std::string, std::string> resp;
            std::vector<std::string> backups;
            std::string secondary_kvs;
            std::string primary_kvs = ip_addr + ":5" + std::to_string(server_group) + std::to_string(server_number) + "0";

            resp["primary"] = primary_kvs;
            server_number++;


            // if (server_groups.count(primary_kvs))
            // {
            server_groups[primary_kvs]["key_range"].push_back(letters.substr(i, 1));
            // }
            // else
            // {
            //     server_groups[primary_kvs]["key_range"] = std::vector<std::string>({letters.substr(i, 1)});
            // }

            while (server_number <= kvs_backups) {
                secondary_kvs = ip_addr + ":5" + std::to_string(server_group) + std::to_string(server_number) + "0";
                resp["secondary"+std::to_string(server_number)] = secondary_kvs;
                server_number++;


                backups.push_back(secondary_kvs);
            }
            kvs_responsibilities[letters[i]] = resp;

            if (!server_groups[primary_kvs].count("backups")) server_groups[primary_kvs]["backups"] = backups;

            i++;
        }
        else
        {
            server_group++;
        }
    }

    for (auto &res: kvs_responsibilities)
    {
        std::string msg = std::string(1, res.first)+" ";
        for (auto &backups: res.second)
        {
            msg += backups.first + "=" + backups.second + " ";
        }
        logger.log(msg, LOGGER_DEBUG);
    }

    for (auto &group: server_groups)
    {
        std::string msg = group.first + " <";
        for (i=0; i < group.second["backups"].size(); i++) msg += group.second["backups"][i] + (i != group.second["backups"].size() - 1 ? ", ": "");
        msg += "> <";
        for (i=0; i < group.second["key_range"].size(); i++) msg += group.second["key_range"][i];
        msg += ">";
        logger.log(msg, LOGGER_DEBUG);
    }



    // coordinator runs on 127.0.0.1:4999
    return 0;
}