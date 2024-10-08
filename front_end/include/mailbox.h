#ifndef MAILBOX_H
#define MAILBOX_H

#include <iostream>
#include <thread>
#include <map>
#include <set>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <vector>
#include <utility>
#include <sstream>
#include <string>
#include <unordered_map>
#include <regex>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/md5.h>
#include <iomanip>
#include <algorithm>


#include "email_data.h"

#include "../../http_server/include/http_server.h"
#include "../utils/include/fe_utils.h"

#include "smtp_client.h" // relay

/**
 * Definitions and utility functions for handling email operations over HTTP.
 */

//helper functions for mailbox 
// Parses a path to extract the mailbox row key in the format "user1-mbox/"
std::string parseMailboxPathToRowKey(const std::string& path);

// Checks if the vector starts with the given prefix
bool startsWith(const std::vector<char>& vec, const std::string& prefix);

/// @brief helper function that parses email body after retrieval from KVS
/// @param kvs_response response from retrieving a valid email from KVS
/// @return reutrns an unordered map of email components 
std::unordered_map<std::string, std::string> parseEmailBody(std::vector<char> kvs_response);

// Handlers for different email operations
void forwardEmail_handler(const HttpRequest& request, HttpResponse& response);
void replyEmail_handler(const HttpRequest& request, HttpResponse& response);
void deleteEmail_handler(const HttpRequest& request, HttpResponse& response);
void sendEmail_handler(const HttpRequest& request, HttpResponse& response);
void email_handler(const HttpRequest& request, HttpResponse& response);
void mailbox_handler(const HttpRequest& request, HttpResponse& response);
void compose_email(const HttpRequest& request, HttpResponse& response);

#endif // EMAIL_SERVER_H
