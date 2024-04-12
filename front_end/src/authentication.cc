/*
 * authentication.cc
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 */

#include "../include/authentication.h"

// function to handle signup requests
void signup_handler(const HttpRequest& req, HttpResponse& res)
{
    
}

// function to handle login requests
void login_handler(const HttpRequest& req, HttpResponse& res)
{
    Logger logger("Login Handler");

    // if request is POST
    if (req.method.compare("POST") == 0) 
    {
        logger.log("Received POST request", LOGGER_INFO);

        // get request body
        std::string req_body = req.body_as_string();

        // parse username and password from request body
        std::string username;    // username
        std::string password;    // password
        std::vector<std::string> kvs_addr;
        std::vector<std::string> req_headers;


        // check if we know already know the KVS server address for user
        if (HttpServer::user_backend_address.count(username))
        {
            kvs_addr = HttpServer::user_backend_address.at(username);
        }
        // otherwise get KVS server address from coordinator
        else
        {
            // // create socket with known coordinator ip:port
            // // return fd for socket

            // // query the coordinator for the KVS server address
            // kvs_addr = query_coordinator(username);

            // // close coordinator socket 
        }

        // create socket for communication with KVS server
        int kvs_sock = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

        // get sid from 
        std::vector<char> row_key(username.begin(), username.end());
        std::string c_key_str = "pass";
        std::vector<char> col_key(c_key_str.begin(), c_key_str.end());
        std::vector<char> kvs_sid = FeUtils::kv_get(kvs_sock, row_key, col_key);
        std::string sid(kvs_sid.begin(), kvs_sid.end());
        
        
        req_headers = req.get_header("Cookie");

        
        // send GET(r,c) to kvs to retrieve user password


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


// function to handle logout requests
void logout_handler(const HttpRequest& req, HttpResponse& res)
{

}

// helper function that validates sessionID of a user
bool validate_sessionID(std::string& username, int sid)
{

}

// function to handle password update requests
void update_password_handler(const HttpRequest& req, HttpResponse& res)
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