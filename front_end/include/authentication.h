/*
 * authentication.h
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 * 
 *  Handler and helper functions that handle authentication and session management
 */

#ifndef FRONT_END_INCLUDE_AUTHENTICATION_H_
#define FRONT_END_INCLUDE_AUTHENTICATION_H_

#include <cstring>
#include <random>
#include <openssl/sha.h>
#include "../utils/include/fe_utils.h"
#include "../../http_server/include/http_server.h"


/// @brief handles new user signup requests on /api/signup route
/// @param req HttpRequest object
/// @param res HttpResponse object
void signup_handler(const HttpRequest& req, HttpResponse& res);

/// @brief handles login requests on /api/login route
/// @param req HttpRequest object
/// @param res HttpResponse object
void login_handler(const HttpRequest& req, HttpResponse& res);

/// @brief handles logout requests on /api/logout route
/// @param req HttpRequest object
/// @param res HttpResponse object
void logout_handler(const HttpRequest& req, HttpResponse& res);

/// @brief handles password change requests on /api/pass_change route
/// @param req HttpRequest object
/// @param res HttpResponse object
void update_password_handler(const HttpRequest& req, HttpResponse& res);

/// @brief validates the password against the KVS given the username using cryptographically secure challenge-response protocol
/// @param kvs_fd file descriptor for KVS server
/// @param username username associated with current client session
/// @param password password associated with current client session
/// @return true if password is associated with valid user; false otherwise
bool validate_password(int kvs_fd, std::string& username, std::string& password);

/// @brief handles password change requests on /api/pass_change route
/// @param req HttpRequest object
/// @param res HttpResponse object
void update_password_handler(const HttpRequest& req, HttpResponse& res);

/// @brief validates the password against the KVS given the username using cryptographically secure challenge-response protocol
/// @param kvs_fd file descriptor for KVS server
/// @param username username associated with current client session
/// @param password password associated with current client session
/// @return true if password is associated with valid user; false otherwise
bool validate_password(int kvs_fd, std::string& username, std::string& password);

/// @brief helper function generating 32-byte hash string using SHA256 cryptographic hash algorithm
/// @param string string to be hashed
/// @param outputBuffer buffer to store resulting hash
void sha256(char *string, char outputBuffer[65]);

/// @brief helper function that generates a random challenge in the form of a string
/// @param length length of the challenge to be generated
/// @return returns the generated challenge
std::string generate_challenge(std::size_t length);

/// @brief helper function that generates secure random session IDs
/// @return returns the generated session ID
std::string generate_sid();

/// @brief helper function that generates a welcome email for new users and stores
/// @return a <email header, email body> as a vector that stores vectors of chars
std::vector<std::vector<char>> generate_welcome_mail();

#endif /* FRONT_END_INCLUDE_AUTHENTICATION_H_ */
