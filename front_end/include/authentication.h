/*
 * authentication.h
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 */

#ifndef FRONT_END_INCLUDE_AUTHENTICATION_H_
#define FRONT_END_INCLUDE_AUTHENTICATION_H_

#include <cstring>
#include <random>
#include <openssl/sha.h>
#include "../utils/include/fe_utils.h"
#include "../../http_server/include/http_server.h"


// function to handle signup requests
void signup_handler(const HttpRequest& req, HttpResponse& res);

// function to handle login requests
void login_handler(const HttpRequest& req, HttpResponse& res);

// function to handle logout requests
void logout_handler(const HttpRequest& req, HttpResponse& res);

// function to handle password update requests
void update_password_handler(const HttpRequest& req, HttpResponse& res);

// helper function that validates the password of a user 
// using a cryptographically secure challenge-response protocol
bool validate_password(int kvs_fd, std::string& username, std::string& password);

// helper function for hashing using SHA256 algorithm
void sha256(char *string, char outputBuffer[65]);

// helper function that generates random strings
std::string generate_challenge(std::size_t length);

// helper function that generates secure random session IDs
std::string generate_sid();


#endif /* FRONT_END_INCLUDE_AUTHENTICATION_H_ */
