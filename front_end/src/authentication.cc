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
    // TODO: VALIDATE THAT INPUT IS LOWER CASE
    // VALIDATE THAT PASSWORD IS WITHIN LENGTH REQUIREMENTS

    // Setup logger
    Logger logger("SignUp Handler");
    logger.log("Received POST request", LOGGER_INFO);

    // bad API request / signup attempt with no username=<value> or password=<value> pattern
    if (req.body_as_string().find("&") == std::string::npos)
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/400");

        return;
    }

    // get request body
    std::vector<std::string> req_body = Utils::split(req.body_as_string(), "&");

    // bad API request / signup attempt with no username=<value> or password=<value> pattern
    if (req_body[0].find("=") == std::string::npos || req_body[1].find("=") == std::string::npos)
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/400");

        return;
    }

    // parse username and password from request body
    std::string username = Utils::trim(Utils::split(req_body[0], "=")[1]);
    std::string password = Utils::trim(Utils::split(req_body[1], "=")[1]);

    // check signup constraints
    if (username.empty() || password.empty() || password.size() < 4 || password.size() > 20)
    {
        // set response status code - 400 bad api request
        res.set_code(303);

        // set response headers
        res.set_header("Location", "/400");

        return;
    }

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
        kvs_res = FeUtils::kv_put(kvs_sock, row_key, std::vector<char>({'p', 'a', 's', 's'}), pass_hash);                            // store hashed password
        kvs_res = FeUtils::kv_put(kvs_sock, row_key, std::vector<char>({'s', 'i', 'd'}), std::vector<char>(sid.begin(), sid.end())); // store current session id

        // create user mailbox
        std::vector<std::vector<char>> welcome_email = generate_welcome_mail();
        std::string mail_suffix = "-mailbox/";
        row_key.pop_back(); // remove backslash on username
        row_key.insert(row_key.end(), mail_suffix.begin(), mail_suffix.end());
        kvs_res = FeUtils::kv_put(kvs_sock, row_key, welcome_email[0], welcome_email[1]);

        // cache kvs server for user
        HttpServer::set_kvs_addr(username, kvs_addr[0] + ":" + kvs_addr[1]);

        // set cookies
        FeUtils::set_cookies(res, username, sid);

        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Location", "/home");
    }
    // account already exists - 409 conflict
    else
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/409");
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

    // bad API request / login attempt with no username&password pattern
    if (req.body_as_string().find("&") == std::string::npos)
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/400");

        return;
    }

    // get request body
    std::vector<std::string> req_body = Utils::split(req.body_as_string(), "&");

    // bad API request / login attempt with no username=<value> or password=<value> pattern
    if (req_body[0].find("=") == std::string::npos || req_body[1].find("=") == std::string::npos)
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/400");

        return;
    }

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

    // check password
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
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/home");
    }
    // invalid login
    else
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Content-Type", "text/html");
        res.set_header("Location", "/401");
    }

    // close socket for KVS server
    close(kvs_sock);
}

/// @brief home page after authentication
/// @param req HttpRequest object
/// @param res HttpResponse object
void home_page(const HttpRequest &req, HttpResponse &res)
{
    Logger logger("Home");
    logger.log("Resource Requested!", LOGGER_DEBUG);
    // get cookies
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);
    if (cookies.count("user") && cookies.count("sid"))
    {
        FeUtils::set_cookies(res, cookies["user"], cookies["sid"]);

        std::string page =
            "<!doctype html>"
            "<html lang='en' data-bs-theme='dark'>"
            "<head>"
            "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
            "<meta content='utf-8' http-equiv='encoding'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<meta name='description' content='CIS 5050 Spr24'>"
            "<meta name='keywords' content='Home'>"
            "<title>Home - PennCloud.com</title>"
            "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
            "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
            "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
            "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
            "</head>"

            "<body onload='setTheme()'>"
            "<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
            "<div class='container-fluid'>"
            "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
            "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
            "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
            "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
            "</svg>"
            "PennCloud"
            "</span>"
            "<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup' aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
            "<span class='navbar-toggler-icon'></span>"
            "</button>"
            "<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
            "<div class='navbar-nav'>"
            "<a class='nav-link active' aria-current='page' href='/home'>Home</a>"
            "<a class='nav-link' href='/drive/" +
            cookies["user"] + "/'>Drive</a>"
                              "<a class='nav-link' href='/" +
            cookies["user"] + "/mbox'>Email</a>"
                              "<a class='nav-link disabled' aria-disabled='true'>Games</a>"
                              "<a class='nav-link' href='/account'>Account</a>"
                              "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
                              "<input type='hidden' />"
                              "<button class='btn nav-link' type='submit'>Logout</button>"
                              "</form>"
                              "</div>"
                              "</div>"
                              "<div class='form-check form-switch form-check-reverse'>"
                              "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
                              "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
                              "</div>"
                              "</div>"
                              "</nav>"

                              "<div class='container-fluid text-start'>"
                              "<div class='row mx-2 mt-3 mb-4'>"
                              "<h1 class='display-6'>"
                              "Welcome back, " +
            cookies["user"] +
            "!</h1>"
            "</div>"
            "</div>"

            "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
            "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
            "crossorigin='anonymous'></script>"
            "<script>"
            "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
            "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
            "document.documentElement.setAttribute('data-bs-theme', 'light');"
            "$('#switchLabel').html('Light Mode');"
            "sessionStorage.setItem('data-bs-theme', 'light');"
            ""
            "}"
            "else {"
            "document.documentElement.setAttribute('data-bs-theme', 'dark');"
            "$('#switchLabel').html('Dark Mode');"
            "sessionStorage.setItem('data-bs-theme', 'dark');"
            "}"
            "});"
            "</script>"
            "<script>"
            "function setTheme() {"
            "var theme = sessionStorage.getItem('data-bs-theme');"
            "if (theme !== null) {"
            "if (theme === 'dark') {"
            "document.documentElement.setAttribute('data-bs-theme', 'dark');"
            "$('#switchLabel').html('Dark Mode');"
            "$('#flexSwitchCheckReverse').attr('checked', true);"
            "}"
            "else {"
            "document.documentElement.setAttribute('data-bs-theme', 'light');"
            "$('#switchLabel').html('Light Mode');"
            "$('#flexSwitchCheckReverse').attr('checked', false);"
            "}"
            "}"
            "};"
            "</script>"
            "</body>"
            "</html>";
        res.set_code(200);
        res.append_body_str(page);
        res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
        res.set_header("Pragma", "no-cache");
        res.set_header("Expires", "0");
    }
    // unauthorized
    else
    {
        res.set_code(303);
        res.set_header("Location", "/401");
    }
}

/// @brief account page after authentication
/// @param req HttpRequest object
/// @param res HttpResponse object
void update_password_page(const HttpRequest &req, HttpResponse &res)
{
    // get cookies
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);
    if (cookies.count("user") && cookies.count("sid"))
    {
        FeUtils::set_cookies(res, cookies["user"], cookies["sid"]);

        std::string page =
            "<!doctype html>"
            "<html lang='en' data-bs-theme='dark'>"
            "<head>"
            "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
            "<meta content='utf-8' http-equiv='encoding'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<meta name='description' content='CIS 5050 Spr24'>"
            "<meta name='keywords' content='SignUp'>"
            "<title>Sign Up - PennCloud.com</title>"
            "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
            "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
            "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
            "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
            "</head>"
            "<body onload='setTheme()'>"
            "<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
            "<div class='container-fluid'>"
            "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
            "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
            "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
            "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
            "</svg>"
            "PennCloud"
            "</span>"
            "<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup'"
            "aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
            "<span class='navbar-toggler-icon'></span>"
            "</button>"
            "<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
            "<div class='navbar-nav'>"
            "<a class='nav-link' href='/home'>Home</a>"
            "<a class='nav-link' href='/drive/"+ cookies["user"] + "/'>Drive</a>"
            "<a class='nav-link' href='/" + cookies["user"] + "/mbox'>Email</a>"
            "<a class='nav-link disabled' aria-disabled='true'>Games</a>"
            "<a class='nav-link active' aria-current='page' href='/account'>Account</a>"
            "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
            "<input type='hidden' />"
            "<button class='btn nav-link' type='submit'>Logout</button>"
            "</form>"
            "</div>"
            "</div>"
            "<div class='form-check form-switch form-check-reverse'>"
            "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
            "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
            "</div>"
            "</div>"
            "</nav>"
            "<div class='container-fluid'>"
            "<div class='row mx-2 mt-3 mb-4'>"
            "<h1 class='display-6'>"
            "Update Password"
            "</h1>"
            "</div>"
            "<div class='row mt-2 mx-2'>"
            "<div class='col-3'></div>"
            "<div class='col-6'>"
            "<form action='/api/update_password' method='POST'>"
            "<div class='form-group'>"
            "<div class='mb-3'>"
            "<label for='inputPassword' class='form-label'>New Password</label>"
            "<input type='password' class='form-control' id='inputPassword' aria-describedby='passHelp'"
            "name='password' required maxlength='20' minlength='4'>"
            "<div id='passHelp' class='form-text'>Must be 4-20 characters long</div>"
            "</div>"
            "<div class='col-12'>"
            "<button class='btn btn-primary' type='submit'>Update</button>"
            "</div>"
            "</div>"
            "</form>"
            "</div>"
            "<div class='col-3'></div>"
            "</div>"
            "</div>"

            "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
            "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
            "crossorigin='anonymous'></script>"
            "<script>"
            "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
            "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
            "document.documentElement.setAttribute('data-bs-theme', 'light');"
            "$('#switchLabel').html('Light Mode');"
            "sessionStorage.setItem('data-bs-theme', 'light');"
            ""
            "}"
            "else {"
            "document.documentElement.setAttribute('data-bs-theme', 'dark');"
            "$('#switchLabel').html('Dark Mode');"
            "sessionStorage.setItem('data-bs-theme', 'dark');"
            "}"
            "});"
            "</script>"
            "<script>"
            "function setTheme() {"
            "var theme = sessionStorage.getItem('data-bs-theme');"
            "if (theme !== null) {"
            "if (theme === 'dark') {"
            "document.documentElement.setAttribute('data-bs-theme', 'dark');"
            "$('#switchLabel').html('Dark Mode');"
            "$('#flexSwitchCheckReverse').attr('checked', true);"
            "}"
            "else {"
            "document.documentElement.setAttribute('data-bs-theme', 'light');"
            "$('#switchLabel').html('Light Mode');"
            "$('#flexSwitchCheckReverse').attr('checked', false);"
            "}"
            "}"
            "};"
            "</script>"
            "</body>";

        res.set_code(200);
        res.append_body_str(page);
        res.set_header("Content-Type", "text/html");
        res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
        res.set_header("Pragma", "no-cache");
        res.set_header("Expires", "0");
    }
    // unauthorized
    else
    {
        res.set_code(303);
        res.set_header("Location", "/401");
    }
}

/// @brief account page after successful password update
/// @param req HttpRequest object
/// @param res HttpResponse object
void update_password_success_page(const HttpRequest &req, HttpResponse &res)
{
    // get cookies
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);
    if (cookies.count("user") && cookies.count("sid"))
    {
        FeUtils::set_cookies(res, cookies["user"], cookies["sid"]);

        std::string page =
            "<!doctype html>"
            "<html lang='en' data-bs-theme='dark'>"
            "<head>"
            "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
            "<meta content='utf-8' http-equiv='encoding'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<meta name='description' content='CIS 5050 Spr24'>"
            "<meta name='keywords' content='SignUp'>"
            "<title>Sign Up - PennCloud.com</title>"
            "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
            "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
            "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
            "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
            "</head>"
            "<body onload='setTheme()'>"
            "<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
            "<div class='container-fluid'>"
            "<span class='navbar-brand mb-0 h1 flex-grow-1'>"
            "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
            "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
            "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
            "</svg>"
            "PennCloud"
            "</span>"
            "<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup'"
            "aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
            "<span class='navbar-toggler-icon'></span>"
            "</button>"
            "<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
            "<div class='navbar-nav'>"
            "<a class='nav-link' href='/home'>Home</a>"
            "<a class='nav-link' href='/drive/+" + cookies["user"] + "/'>Drive</a>"
            "<a class='nav-link' href='/" + cookies["user"] + "/mbox'>Email</a>"
            "<a class='nav-link disabled' aria-disabled='true'>Games</a>"
            "<a class='nav-link active' aria-current='page' href='/account'>Account</a>"
            "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
            "<input type='hidden' />"
            "<button class='btn nav-link' type='submit'>Logout</button>"
            "</form>"
            "</div>"
            "</div>"
            "<div class='form-check form-switch form-check-reverse'>"
            "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
            "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
            "</div>"
            "</div>"
            "</nav>"
            "<div class='container-fluid'>"
            "<div class='row mx-2 mt-3 mb-4'>"
            "<div class='col-9'>"
            "<h1 class='display-6'>"
            "Update Password - Successful!"
            "</h1>"
            "</div>"
            "<div class='col-3'>"
            "<h1 class='display-6'>"
            "<svg xmlns='http://www.w3.org/2000/svg' width='1em' height='1em' fill='currentColor' class='bi bi-check2-circle' viewBox='0 0 16 16'>"
            "<path d='M2.5 8a5.5 5.5 0 0 1 8.25-4.764.5.5 0 0 0 .5-.866A6.5 6.5 0 1 0 14.5 8a.5.5 0 0 0-1 0 5.5 5.5 0 1 1-11 0'/>"
            "<path d='M15.354 3.354a.5.5 0 0 0-.708-.708L8 9.293 5.354 6.646a.5.5 0 1 0-.708.708l3 3a.5.5 0 0 0 .708 0z'/>"
            "</svg>"
            "</h1>"
            "</div>"
            "</div>"
            "<div class='row mt-2 mx-2'>"
            "<div class='col-3'></div>"
            "<div class='col-6'>"
            "<form action='/api/update_password' method='POST'>"
            "<div class='form-group'>"
            "<div class='mb-3'>"
            "<label for='inputPassword' class='form-label'>New Password</label>"
            "<input type='password' class='form-control' id='inputPassword' aria-describedby='passHelp'"
            "name='password' required maxlength='20' minlength='4'>"
            "<div id='passHelp' class='form-text'>Must be 4-20 characters long</div>"
            "</div>"
            "<div class='col-12'>"
            "<button class='btn btn-primary' type='submit'>Update</button>"
            "</div>"
            "</div>"
            "</form>"
            "</div>"
            "<div class='col-3'></div>"
            "</div>"
            "</div>"
            "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
            "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
            "crossorigin='anonymous'></script>"
            "<script>"
            "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
            "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
            "document.documentElement.setAttribute('data-bs-theme', 'light');"
            "$('#switchLabel').html('Light Mode');"
            "sessionStorage.setItem('data-bs-theme', 'light');"
            ""
            "}"
            "else {"
            "document.documentElement.setAttribute('data-bs-theme', 'dark');"
            "$('#switchLabel').html('Dark Mode');"
            "sessionStorage.setItem('data-bs-theme', 'dark');"
            "}"
            "});"
            "</script>"
            "<script>"
            "function setTheme() {"
            "var theme = sessionStorage.getItem('data-bs-theme');"
            "if (theme !== null) {"
            "if (theme === 'dark') {"
            "document.documentElement.setAttribute('data-bs-theme', 'dark');"
            "$('#switchLabel').html('Dark Mode');"
            "$('#flexSwitchCheckReverse').attr('checked', true);"
            "}"
            "else {"
            "document.documentElement.setAttribute('data-bs-theme', 'light');"
            "$('#switchLabel').html('Light Mode');"
            "$('#flexSwitchCheckReverse').attr('checked', false);"
            "}"
            "}"
            "};"
            "</script>"
            "</body>";

        res.set_code(200);
        res.append_body_str(page);
        res.set_header("Content-Type", "text/html");
        res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
        res.set_header("Pragma", "no-cache");
        res.set_header("Expires", "0");
    }
    // unauthorized
    else
    {
        res.set_code(303);
        res.set_header("Location", "/401");
    }
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
        std::vector<char> col_key({'s', 'i', 'd'});
        std::vector<char> kvs_res = FeUtils::kv_put(kvs_sock, row_key, col_key, std::vector<char>({'-', '1'}));

        // clear user from local cache of kvs addresses
        if (present)
            HttpServer::delete_kvs_addr(username);

        // invalidated cookies
        FeUtils::expire_cookies(res, username, sid);

        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Location", "/login.html");

        // close socket for KVS server
        close(kvs_sock);
    }
    // if either cookie expired or was never set
    else
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Location", "/401"); // unathorized
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
            res.set_code(303);

            // set response headers
            res.set_header("Content-Type", "text/html");
            res.set_header("Location", "/update_success"); // TODO: Validate
        }
        // otherwise send them back to login page
        else
        {
            // set response status code
            res.set_code(303);

            // set response headers
            res.set_header("Location", "/401");
        }

        close(kvs_sock);
    }
    else
    {
        // set response status code
        res.set_code(303);

        // set response headers
        res.set_header("Location", "/401");
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

    return std::vector<std::vector<char>>({header, body});
}
