/*
 * authentication.cc
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 * 
 *  Handler and helper functions that handle authentication and session management
 */

#include "../include/authentication.h"

// https://www.w3schools.com/tags/att_form_enctype.asp may be important once we are building upload fields for files

/// @brief handles new user signup requests on /api/signup route
/// @param req HttpRequest object
/// @param res HttpResponse object
void signup_handler(const HttpRequest &req, HttpResponse &res)
{
    // Setup logger
    Logger logger("SignUp Handler");
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

    // check if user has a password in KVS
    logger.log("Username = " + username, LOGGER_DEBUG); // DEBUG
    std::vector<char> row_key(username.begin(), username.end());
    row_key.push_back('/');
    std::vector<char> kvs_res = FeUtils::kv_get(kvs_sock, row_key, std::vector<char>({'p', 'a', 's', 's'}));
    
    // if no username match
    if (!FeUtils::kv_success(kvs_res))
    {
        // hash password
        std::vector<char> pass_hash(65);
        sha256(&password[0], &pass_hash[0]);
        
        // generate new session id for the user
        std::string sid = generate_sid();

        // create new user
        kvs_res = FeUtils::kv_put(kvs_sock, row_key, std::vector<char>({'p', 'a', 's', 's'}), pass_hash); // store hashed password
        kvs_res = FeUtils::kv_put(kvs_sock, row_key, std::vector<char>({'s', 'i', 'd'}), std::vector<char>(sid.begin(), sid.end())); // store current session id
        
        // create user mailbox
        std::vector<std::vector<char>> welcome_email = generate_welcome_mail();
        std::string mail_suffix = "-mailbox/";
        row_key.pop_back(); // remove backslash on username
        row_key.insert(row_key.end(), mail_suffix.begin(), mail_suffix.end());
        kvs_res = FeUtils::kv_put(kvs_sock, row_key, welcome_email[0], welcome_email[1]);

        // cache kvs server for user
        HttpServer::set_kvs_addr(username, kvs_addr[0]+":"+kvs_addr[1]);

        // set cookies
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
            "User successfully created, redirecting to home page!"
            "</body>"
            "</html>";
        res.append_body_str(html);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/home");

    }
    else
    {
        logger.log("User already exists: " + std::string(kvs_res.begin(), kvs_res.end()), LOGGER_DEBUG);

        // set response status code
        res.set_code(400);

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
            "User already exists, redirecting to login!"
            "</body>"
            "</html>";
        res.append_body_str(html);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/");
    }

    close(kvs_sock);
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
        // session is inactive - redirect to login page
        else
        {
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
            res.set_header("Location", "/");
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
    // Setup logger
    Logger logger("Logout Handler");
    logger.log("Received POST request", LOGGER_INFO);

    // setup cookies
    std::string username;
    std::string sid;

    // parse cookies
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);

    // if cookies are present then they are valid
    if (cookies.count("user") && cookies.count("sid"))
    {
        // get relevant cookies
        username = cookies["user"];
        sid = cookies["sid"];

        // check if user exists in cache
        bool present = HttpServer::check_kvs_addr(username);
        std::vector<std::string> kvs_addr;

        // get the KVS server address for user associated with the request
        if (present)
        {
            // get address from cache
            kvs_addr = HttpServer::get_kvs_addr(username);
        }
        else
        {
            // query the coordinator for the KVS server address
            kvs_addr = FeUtils::query_coordinator(username);
        }

        // create socket for communication with KVS server
        int kvs_sock = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

        // delete associated sid from kvs server
        std::vector<char> row_key(username.begin(), username.end());
        row_key.push_back('/');
        std::vector<char> col_key(sid.begin(), sid.end());
        std::vector<char> kvs_res = FeUtils::kv_put(kvs_sock, row_key, col_key, std::vector<char>({'-', '1'}));

        // clear user from local cache of kvs addresses
        if (present)
            HttpServer::delete_kvs_addr(username);

        // invalidated cookies
        FeUtils::expire_cookies(res, username, sid);

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
            "Good, Redirecting to Login!"
            "</body>"
            "</html>";
        res.append_body_str(html);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/");

        // close socket for KVS server
        close(kvs_sock);
    }
    // if either cookie expired or was never set
    else
    {
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
            "Bad, Go Login!"
            "</body>"
            "</html>";
        res.append_body_str(html);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/");
    }
}

/// @brief handles password change requests on /api/pass_change route
/// @param req HttpRequest object
/// @param res HttpResponse object
void update_password_handler(const HttpRequest &req, HttpResponse &res)
{
    // Setup logger
    Logger logger("Password Update Handler");
    logger.log("Received POST request", LOGGER_INFO);

    // parse cookies
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);

    // if cookies are present, validate them
    if (cookies.count("user") && cookies.count("sid"))
    {
        // get relevant cookies
        std::string username = cookies["user"];
        std::string sid = cookies["sid"];

        // check if user exists in cache
        bool present = HttpServer::check_kvs_addr(username);
        std::vector<std::string> kvs_addr;

        // get the KVS server address for user associated with the request
        if (present)
        {
            // get address from cache
            kvs_addr = HttpServer::get_kvs_addr(username);
        }
        else
        {
            // query the coordinator for the KVS server address
            kvs_addr = FeUtils::query_coordinator(username);
        }

        // create socket for communication with KVS server
        int kvs_sock = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

        // validate session id
        std::string valid_session_id = FeUtils::validate_session_id(kvs_sock, username, req);

        // check if session is valid
        if (!valid_session_id.empty())
        {
            // get request body
            std::vector<std::string> req_body = Utils::split(req.body_as_string(), "&");

            // parse new password from request body and compute its hash
            std::string new_password = Utils::trim(Utils::split(req_body[0], "=")[1]);
            std::vector<char> new_pass_hash(65);
            sha256(&new_password[0], &new_pass_hash[0]);

            // store hash of new password in the kvs
            std::vector<char> row_key(username.begin(), username.end());
            row_key.push_back('/');
            std::vector<char> kvs_res = FeUtils::kv_put(
                kvs_sock,
                row_key,
                std::vector<char>({'p', 'a', 's', 's'}),
                new_pass_hash);

            // if not present, set cache
            if (!present)
                HttpServer::set_kvs_addr(username, kvs_addr[0] + ":" + kvs_addr[1]);

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
                "Password Successfully Updated!"
                "</body>"
                "</html>";
            res.append_body_str(html);

            // set response headers
            res.set_header("Content-Type", "text/html");
            res.set_header("Location", "/pass_change"); // TODO: Validate
        }
        // otherwise send them back to login page
        else
        {
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
                "Session Expired, Please Reauthenticate!"
                "</body>"
                "</html>";
            res.append_body_str(html);

            // set response headers
            res.set_header("Content-Type", "text/html");
            res.set_header("Location", "/");
        }

        close(kvs_sock);
    }
    else
    {
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
            "Session Expired, Please Reauthenticate!"
            "</body>"
            "</html>";
        res.append_body_str(html);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/");
    }
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

/// @brief helper function that generates a welcome email for new users and stores
/// @return a <email header, email body> as a vector that stores vectors of chars
std::vector<std::vector<char>> generate_welcome_mail()
{
    std::vector<char> header;
    std::vector<char> body;

    return std::vector<std::vector<char>>({header,body});
}

