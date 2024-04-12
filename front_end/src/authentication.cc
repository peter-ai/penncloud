/*
 * authentication.cc
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 */

#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <random>
#include <openssl/sha.h>
#include "../include/authentication.h"
#include "../../http_server/include/http_server.h"
#include "../../http_server/include/http_request.h"
#include "../../http_server/include/http_response.h"



// function to handle login requests
void login_handler(const HttpRequest& req, HttpResponse& res)
{
    // if request is POST
    if (req.req_method.compare("POST") == 0) 
    {
        // get request body
        std::string req_body(req.body.begin(), req.body.end());

        // parse username and password from request body
        std::string username;    // username
        std::string password;    // password

        // create socket with known coordinator ip:port
            // return fd for socket

        // send username to coordinator and find relevant tablet server
            // return the ip:port of the tablet server

        // close coordinator socket 
            // close(fd);

        // create socket with known tablet server ip:port
            // return fd for socket
        
        // send GET(r,c) to kvs to retrieve user password
        std::string r_key_str = "authentication";
        std::vector<char> row_key(r_key_str.begin(), r_key_str.end());
        std::vector<char> col_key(username.begin(), username.end());
        std::vector<char> kvs_res; // = kv_get(sock_fd, row_key, col_key);

        // if good response from KVS
        if ((kvs_res[0] == '+') && (kvs_res[1] == 'O') && (kvs_res[2] == 'K'))
        {
            // get stored password hash out of kvs response
            std::string kvs_pass_hash(kvs_res.begin()+4, kvs_res.end());

            // generate random challenge
            std::string challenge = generate_challenge(64);

            // concat kvs stored password hash with random challenge
            std::string kvs_pass_chall = challenge + kvs_pass_hash;

            // compute hash of given password and concat with random challenge
            std::vector<char> password_hash(65);
            sha256(&password[0], password_hash.data());
            std::string client_pass_chall = challenge + std::string(password_hash.begin(), password_hash.end());



        }
        else
        {

        }



        // construct html page from retrieved data
        std::string html = 
        "<!doctype html>" 
            "<html>"
                "<head>"
                    "<title>Our Funky HTML Page</title>"
                    "<meta name='description' content='Our first page'>"
                    "<meta name='keywords' content='html tutorial template'>"
                "</head>"
                "<body>"
                    "Content goes here."
                "</body>"
            "</html>";
        std::vector<char> webpage(html.begin(), html.end());
    }
    else
    {
        // bad request
    }
}


// function to handle signup requests
void signup_handler(const HttpRequest& req, HttpResponse& res)
{
    
}


// helper function for hashing using SHA256 algorithm
void sha256(char *string, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256_hash;
    SHA256_Init(&sha256_hash);
    SHA256_Update(&sha256_hash, string, strlen(string));
    SHA256_Final(hash, &sha256_hash);
    int i = 0;
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

// helper function that generates random strings
std::string generate_challenge(std::size_t length)
{
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;

    for (std::size_t i = 0; i < length; ++i)
    {
        random_string += CHARACTERS[distribution(generator)];
    }

    return random_string;
}