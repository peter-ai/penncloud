/*
 * authentication.cc
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 */

#include "../include/authentication.h"

// https://www.w3schools.com/tags/att_form_enctype.asp may be important once we are building upload fields for files

/// @brief handles new user signup requests on /api/signup route
/// @param req HttpRequest object
/// @param res HttpResponse object
void signup_handler(const HttpRequest &req, HttpResponse &res)
{
    (void)req;
    (void)res;
    // on signup
    // check if user exists,
    // if they do, send them to login page with special messaging
    // otherwise, create primary folder user/ with col pass
    // create empty sid at r:user/ c:sid v=''
    // create empty mailbox at user/ mailbox v=metadata
}

/// @brief handles login requests on /api/login route
/// @param req HttpRequest object
/// @param res HttpResponse object
void login_handler(const HttpRequest &req, HttpResponse &res)
{
    // Setup logger
    Logger logger("Login Handler");
    logger.log("Received POST request", LOGGER_INFO);

    // get request body
    std::vector<std::string> req_body = Utils::split(req.body_as_string(), "&");

    // parse username and password from request body
    std::string username = Utils::trim(Utils::split(req_body[0], "=")[1]);
    std::string password = Utils::trim(Utils::split(req_body[1], "=")[1]);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int kvs_sock = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    std::string valid_session_id = FeUtils::validate_session_id(kvs_sock, username, req);
    logger.log("Client session ID: " + (valid_session_id.empty() ? "[empty]" : "[" + valid_session_id + "]"), LOGGER_INFO);

    // if there is a valid session id, then construct response and redirect user
    if (!valid_session_id.empty())
    {
        // if not present, set cache kvs address for the current user
        if (!present)
            HttpServer::set_kvs_addr(username, kvs_addr[0] + ":" + kvs_addr[1]);

        // set cookies on response
        FeUtils::set_cookies(res, username, valid_session_id);

        // set response status code
        res.set_code(200);

        // construct html page from retrieved data and set response body
        std::string html =
            "<!doctype html>"
            "<html>"
            "<head>"
            "<title>PennCloud.com</title>"
            "<meta name='description' content='CIS 5050 Spr24'>"
            "<meta name='keywords' content='HomePage'>"
            "</head>"
            "<body>"
            "Successful Login!"
            "</body>"
            "</html>";
        res.append_body_str(html);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/home"); // TODO: Validate
    }
    // otherwise check password
    else
    {
        // validate password
        if (validate_password(kvs_sock, username, password))
        {
            // if not present, set cache
            if (!present)
                HttpServer::set_kvs_addr(username, kvs_addr[0] + ":" + kvs_addr[1]);

            // generate random SID
            std::string sid = generate_sid();

            // store new sid in the kvs
            std::vector<char> row_key(username.begin(), username.end());
            row_key.push_back('/');
            std::vector<char> kvs_res = FeUtils::kv_put(
                kvs_sock,
                row_key,
                std::vector<char>({'s', 'i', 'd'}),
                std::vector<char>(sid.begin(), sid.end()));

            // set cookies on response
            FeUtils::set_cookies(res, username, sid);

            // set response status code
            res.set_code(200);

            // construct html page from retrieved data and set response body
            std::string html =
                "<!doctype html>"
                "<html>"
                "<head>"
                "<title>PennCloud.com</title>"
                "<meta name='description' content='CIS 5050 Spr24'>"
                "<meta name='keywords' content='HomePage'>"
                "</head>"
                "<body>"
                "Successful Login!"
                "</body>"
                "</html>";
            res.append_body_str(html);

            // set response headers
            res.set_header("Content-Type", "text/html");
            res.set_header("Location", "/home"); // TODO: Validate
        }
        else
        {
            // TODO: return user to shadow realm because authentication was unsuccesful
            // set response status code
            res.set_code(401);

            // construct html page from retrieved data and set response body
            std::string html =
                "<!doctype html>"
                "<html>"
                "<head>"
                "<title>PennCloud.com</title>"
                "<meta name='description' content='CIS 5050 Spr24'>"
                "<meta name='keywords' content='HomePage'>"
                "</head>"
                "<body>"
                "Bad Login!"
                "</body>"
                "</html>";
            res.append_body_str(html);

            // set response headers
            res.set_header("Content-Type", "text/html");
        }
    }

    // close socket for KVS server
    close(kvs_sock);
}

/// @brief handles logout requests on /api/logout route
/// @param req HttpRequest object
/// @param res HttpResponse object
void logout_handler(const HttpRequest &req, HttpResponse &res)
{
    (void)req;
    (void)res;
}

/// @brief handles password change requests on /api/pass_change route
/// @param req HttpRequest object
/// @param res HttpResponse object
void update_password_handler(const HttpRequest &req, HttpResponse &res)
{
    (void)req;
    (void)res;
}

/// @brief validates the password against the KVS given the username using cryptographically secure challenge-response protocol
/// @param kvs_fd file descriptor for KVS server
/// @param username username associated with current client session
/// @param password password associated with current client session
/// @return true if password is associated with valid user; false otherwise
bool validate_password(int kvs_fd, std::string &username, std::string &password)
{
    // construct row key
    std::vector<char> row_key(username.begin(), username.end());
    row_key.push_back('/');

    // send GET kvs to retrieve user password
    const std::vector<char> kvs_res = FeUtils::kv_get(
        kvs_fd,
        row_key,
        std::vector<char>({'p', 'a', 's', 's'})); // TODO: RETRY???

    // if good response from KVS
    if (FeUtils::kv_success(kvs_res))
    {
        // generate random challenge
        std::string challenge = generate_challenge(64);

        // get stored password hash out of kvs response
        std::string kvs_pass_hash(kvs_res.begin() + 4, kvs_res.end());

        // concat kvs stored password hash with random challenge
        std::string kvs_pass_chall = challenge + kvs_pass_hash;

        // compute hash of given password and concat with random challenge
        std::vector<char> password_hash(65);
        sha256(&password[0], &password_hash[0]);
        std::string client_pass_chall = challenge + std::string(password_hash.begin(), password_hash.end());

        // compute comparative hashes
        sha256(&kvs_pass_chall[0], &password_hash[0]);
        std::string kvs_hash(password_hash.begin(), password_hash.end());
        sha256(&client_pass_chall[0], &password_hash[0]);
        std::string client_hash(password_hash.begin(), password_hash.end());

        // validate password
        if (kvs_hash.compare(client_hash) == 0)
            return true;
        else
            return false;
    }
    else
    {
        return false;
        // TODO: RETRY??? -- while loop?; need a function to just call to get latest info from coordinator and refresh global map, close stale socket and create new one
    }
}

/// @brief helper function generating 32-byte hash string using SHA256 cryptographic hash algorithm
/// @param string string to be hashed
/// @param outputBuffer buffer to store resulting hash
void sha256(char *string, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)string, strlen(string), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        snprintf(outputBuffer + (i * 2), 4, "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

/// @brief helper function that generates a random challenge in the form of a string
/// @param length length of the challenge to be generated
/// @return returns the generated challenge
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

/// @brief helper function that generates secure random session IDs
/// @return returns the generated session ID
std::string generate_sid()
{
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, 9);

    std::string sid;

    for (int i = 0; i < 64; i++)
    {
        sid += std::to_string(distribution(generator));
    }

    return sid;
}