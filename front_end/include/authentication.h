/*
 * authentication.h
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 */

#ifndef FRONT_END_INCLUDE_AUTHENTICATION_H_
#define FRONT_END_INCLUDE_AUTHENTICATION_H_

#include <string>
#include <vector>
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"

// function to handle login requests
void login_handler(const HttpRequest& req, HttpResponse& res);

// function to handle signup requests
void signup_handler(const HttpRequest& req, HttpResponse& res);

// helper function for hashing using SHA256 algorithm
void sha256(char *string, char outputBuffer[65]);

// helper function that generates random strings
std::string generate_challenge(std::size_t length);


#endif /* FRONT_END_INCLUDE_AUTHENTICATION_H_ */
