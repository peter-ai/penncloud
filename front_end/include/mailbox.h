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
#include "../../http_server/include/http_server.h"
#include "../utils/include/fe_utils.h"


/**
 * Definitions and utility functions for handling email operations over HTTP.
 */

// Parses a path to extract the mailbox row key in the format "user1-mbox/"
std::string parseMailboxPathToRowKey(const std::string& path);

// Checks if the vector starts with the given prefix
bool startsWith(const std::vector<char>& vec, const std::string& prefix);

// Retrieves a query parameter value from a given HttpRequest
std::string get_query_parameter(const HttpRequest& request, const std::string& key);

// Handlers for different email operations
void forwardEmail_handler(const HttpRequest& request, HttpResponse& response);
void replyEmail_handler(const HttpRequest& request, HttpResponse& response);
void deleteEmail_handler(const HttpRequest& request, HttpResponse& response);
void sendEMail_handler(const HttpRequest& request, HttpResponse& response);
void email_handler(const HttpRequest& request, HttpResponse& response);
void mailbox_handler(const HttpRequest& request, HttpResponse& response);

#endif // EMAIL_SERVER_H
