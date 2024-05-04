/*
 * admin_main.cc
 *
 *  Created on: Apr 25, 2024
 *      Author: aashok12
 *
 * Runs admin console functionality on an HTTP server
 *
 */

#include <iostream>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <strings.h>
#include <stdlib.h>
#include "../../http_server/include/http_server.h"
#include "../../front_end/utils/include/fe_utils.h"
#include "../../utils/include/utils.h"
#include "../../http_server/include/http_server.h"

Logger logger("Admin Console"); // setup logger

using namespace std;
// Dummy global variables for load balancer and coordinator
std::string loadBalancerIP = "127.0.0.1";
int loadBalancerPort = 8081;
std::string coordinatorIP = "127.0.0.1";
int coordinatorPort = 8082;

// bool flags
bool coord_init = false; // flag checks whether coord sent init message
bool lb_init = false;    // flag checks whether lb sent init message

// maps
unordered_map<string, int> lb_servers;                 // maps FE server names to port num
unordered_map<string, int> kvs_servers;                // maps backend server names to port num
unordered_map<string, vector<string>> kvs_servergroup; // server groups to names of all servers within
unordered_map<string, int> server_status;              // @todo easier to do typing with ints? check js


void iterateUnorderedMapOfStringToInt(const std::unordered_map<std::string, int> &myMap)
{
    for (const auto &pair : myMap)
    {
        std::cout << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
    }
}

void iterateMapOfStringToVectorOfString(const std::unordered_map<std::string, std::vector<std::string>> &myMap)
{
    for (const auto &pair : myMap)
    {
        std::cout << "Key: " << pair.first << ", Values:" << std::endl;
        for (const auto &value : pair.second)
        {
            std::cout << "- " << value << std::endl;
        }
    }
}

void toggle_component_handler(const HttpRequest &req, HttpResponse &res)
{
    // Extract form parameters (status and component id) from the HTTP request body
    std::string requestBody = req.body_as_string();
    std::unordered_map<std::string, std::string> formParams;

    // Parse the request body to extract form parameters
    size_t pos = 0;
    while ((pos = requestBody.find('&')) != std::string::npos)
    {
        std::string token = requestBody.substr(0, pos);
        size_t equalPos = token.find('=');
        std::string key = token.substr(0, equalPos);
        std::string value = token.substr(equalPos + 1);
        formParams[key] = value;
        requestBody.erase(0, pos + 1);
    }
    // Handle the last parameter
    size_t equalPos = requestBody.find('=');
    std::string key = requestBody.substr(0, equalPos);
    std::string value = requestBody.substr(equalPos + 1);
    formParams[key] = value;

    // Extract status and component ID from form parameters
    std::string status_str = formParams["status"];
    std::string servername = formParams["component_id"];

    // Convert status to integer
    int status = std::stoi(status_str);

    string msg = "";
    string cmd = "KILL";
    if (status == 0)
    {
        msg = "KILL\r\n";
    }
    else
    {
        msg = "WAKE\r\n";
        cmd = "WAKE";
    }

    // if server is kvs
    if (kvs_servers.find(servername) != kvs_servers.end())
    {
        int port = kvs_servers[servername];
        // int fd = FeUtils::open_socket("127.0.0.1", port);

        //   ssize_t bytes_sent = send(fd, msg.c_str(), msg.size(), 0);
        // temp: @todo uncomment
        ssize_t bytes_sent = 5;
        if (bytes_sent < 0)
        {
            logger.log(cmd + " command could not be sent to KVS server at port " + to_string(port), LOGGER_ERROR);
        }
        else
        {
            logger.log(cmd + " command sent to " + servername + " at port " + to_string(port), LOGGER_INFO);
        }
        server_status[servername] = status;
        // close(fd)
    }
    else if (lb_servers.find(servername) != lb_servers.end())
    {
        int port = lb_servers[servername];
        // int fd = FeUtils::open_socket("127.0.0.1", port);

        // ssize_t bytes_sent = send(fd, msg.c_str(), msg.size(), 0);

        // temp: @todo uncomment
        ssize_t bytes_sent = 5;
        if (bytes_sent < 0)
        {
            logger.log(cmd + " command could not be sent to FE server at port " + to_string(port), LOGGER_ERROR);
        }
        else
        {
            logger.log(cmd + " command sent to " + servername + " at port " + to_string(port), LOGGER_INFO);
        }
        server_status[servername] = status;
        // close(fd)
    }
    else
    {
        logger.log("Server does not exist - " + servername, LOGGER_ERROR);
    }

    // @todo ask B + P abt redirection
    res.set_code(303); // OK
    res.set_header("Location", "/admin/dashboard");
}

void dashboard_handler(const HttpRequest &req, HttpResponse &res)
{
    std::string user = "example"; // Extract user information from cookies if needed

    std::string page =
        "<!doctype html>"
        "<html lang='en' data-bs-theme='dark'>"
        "<head>"
        "<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
        "<meta content='utf-8' http-equiv='encoding'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<meta name='description' content='CIS 5050 Spr24'>"
        "<meta name='keywords' content='Home'>"
        "<title>Admin Console - PennCloud.com</title>"
        "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
        "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
        "integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
        "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
        "<style>"
        "/* Custom CSS for toggle switch */"
        ".toggle-switch {"
        "    width: 30px; /* Adjust width as needed */"
        "    height: 15px; /* Adjust height as needed */"
        "}"
        ""
        ".form-check-label::after {"
        "    width: 15px; /* Adjust width of the switch */"
        "    height: 15px; /* Adjust height of the switch */"
        "    border-radius: 50%; /* Make the switch round */"
        "}"
        "</style>"
        "</head>"

        "<body onload='setTheme()'>"
        "<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
        "<div class='container-fluid'>"
        "<span class='navbar-brand mb-0 h1'>"
        "<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
        "class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
        "<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
        "</svg>"
        "Penn Cloud"
        "</span>"
        "<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup' aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
        "<span class='navbar-toggler-icon'></span>"
        "</button>"
        "<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
        "<div class='navbar-nav'>"
        "<a class='nav-link' href='/home'>Home</a>"
        "<a class='nav-link' href='/drive/" +
        user + "/'>Drive</a>"
               "<a class='nav-link' href='/" +
        user + "/mbox'>Email</a>"
               "<a class='nav-link' href='/account'>Account</a>"
               "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
               "<input type='hidden' />"
               "<button class='btn nav-link' type='submit'>Logout</button>"
               "</form>"
               // "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
               // "<input type='hidden' />"
               // "<button class='btn nav-link' type='submit'>Logout</button>"
               // "</form>"
               "</div>"
               "</div>"
               "<div class='form-check form-switch form-check-reverse ms-auto'>"
               "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
               "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
               "</div>"
               "</div>"
               "</nav>"

               "<div class='container mt-4'>"
               "<h1 class='mb-4'>Admin Console</h1>"
               "<div class='row'>";

    // Frontend Nodes
    page += "<div class='row'>";
    page += "<div class='col'>";
    page += "<h2>Frontend Nodes</h2>";
    for (const auto &server : lb_servers)
    {
        int status = server_status[server.first]; // Retrieve status from server_status using server key

        page += "<div class='form-check form-switch'>";
        page += "<label class='form-check-label' for='" + server.first + "'>" + server.first + "</label>";
        page += "<input type='checkbox' class='form-check-input toggle-switch fe-server-switch' id='" + server.first + "' checked='" + to_string(status) + "'>";
        page += "<label class='form-check-label' for='" + server.first + "'></label>";
        page += "</div>";
        page += "</label>";
    }
    page += "</div>";

    // Key-Value Store Components
    page += "<div class='col'>";
    page += "<h2>Key-Value Store Components</h2>";
    for (int i = 1; i <= 3; ++i)
    {
        page += "<div class='form-check form-switch'>";
        page += "<label class='form-check-label' for='kvs" + std::to_string(i) + "'>KVS" + std::to_string(i) + "</label>";
        page += "<label class='switch'>";
        page += "<input type='checkbox' id='kvs" + std::to_string(i) + "'>";
        page += "<span class='slider round'></span>";
        page += "</label>";
        page += "</div>";
    }
    page += "</div>";
    page += "</div>";

    // Paginated Table
    page += "<div class='mt-4'><h2>Table</h2>";
    page += "<table class='table table-bordered'>";
    page += "<thead><tr><th>ID</th><th>Name</th><th>Value</th></tr></thead>";
    page += "<tbody>";
    for (int i = 1; i <= 10; ++i)
    {
        page += "<tr><td>" + std::to_string(i) + "</td><td>Component</td><td>Value</td></tr>";
    }
    page += "</tbody>";
    page += "</table>";

    // Pagination
    page += "<nav><ul class='pagination justify-content-center'>";
    page += "<li class='page-item'><a class='page-link' href='#'>Previous</a></li>";
    page += "<li class='page-item'><a class='page-link' href='#'>1</a></li>";
    page += "<li class='page-item'><a class='page-link' href='#'>2</a></li>";
    page += "<li class='page-item'><a class='page-link' href='#'>3</a></li>";
    page += "<li class='page-item'><a class='page-link' href='#'>Next</a></li>";
    page += "</ul></nav>";
    page += "</div>";
    page += "</div>"
            "</div>"
            "</div>"
            "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'></script>"
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
            "</script>";
    page += "<script>"
            "$(document).ready(function() {"
            "    $('.fe-server-switch').change(function() {"
            "        var serverName = $(this).attr('id');"
            "        var isChecked = $(this).prop('checked');"
            "        var toggleState = isChecked ? 1 : 0 ;"
            "        $.post('/admin/component/toggle', { serverName: serverName, toggleState: toggleState }, function(data, status) {"
            "            console.log('Toggle status for ' + serverName + ': ' + data);"
            "        });"
            "    });"
            "});"
            "</script>";

    page += "</body>"
            "</html>";

    res.set_code(200);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}

/*
Parses message from coordinator to get KVS server data
*/
void parse_coord_msg(string &msg)
{
    // msg is of format SG#:name<sp>port, name<sp>port,...\n... CRLF

    // split string by new line, each line new SG
    vector<string> kvs_sg_vec = Utils::split(msg, "\n");

    // iterate thru vector, and split again by whitespace
    for (const auto &kvs_sg : kvs_sg_vec)
    {
        // split by ':' to get group name
        vector<string> server_group = Utils::split(kvs_sg, ":");
        string sg_name = server_group[0];
        // split string by commas
        vector<string> fe_serv_strs = Utils::split(server_group[1], ",");

        // iterate thru vector, and split again by whitespace
        for (const auto &fe_str : fe_serv_strs)
        {
            string trimmed = Utils::l_trim(fe_str);
            vector<string> fe_vec = Utils::split(trimmed, " ");
            // put name and port into map
            string server_name = fe_vec[0];
            int port = atoi(fe_vec[1].c_str());
            kvs_servers[server_name] = port;
            // update status
            server_status[server_name] = 1; // 1 for active 0 for killed in status
            kvs_servergroup[sg_name].push_back(server_name);
        }
    }

    // if msg not malformed, update global bool
    coord_init = !kvs_servers.empty();
    iterateUnorderedMapOfStringToInt(kvs_servers);
    iterateMapOfStringToVectorOfString(kvs_servergroup);
}

/*
Parses message from load balancer to get FE servers into map
*/
void parse_lb_msg(string &msg)
{
    // msg is of format name<sp>port, name<sp>port,

    // split string by commas
    vector<string> fe_serv_strs = Utils::split(msg, ",");

    // iterate thru vector, and split again by whitespace
    for (const auto &fe_str : fe_serv_strs)
    {
        string trimmed = Utils::l_trim(fe_str);
        vector<string> fe_vec = Utils::split(trimmed, " ");
        // put name and port into map
        string name = fe_vec[0];
        int port = atoi(fe_vec[1].c_str());
        lb_servers[name] = port;
        // update status
        server_status[name] = 1; // 1 for active 0 for killed in status
    }

    // if msg not malformed, update global bool
    lb_init = !lb_servers.empty();
    iterateUnorderedMapOfStringToInt(lb_servers);
}

/*
Thread function to get server data from LB or Coord
Function constructs map as needed
*/
void get_server_data(int sockfd)
{
    string receivedMessage;
    char buffer[1024]; // Buffer for receiving data
    ssize_t bytesReceived;

    while (true)
    {
        // Receive a message
        bytesReceived = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytesReceived == -1)
        {
            cerr << "Error in receiving message\n";
            break;
        }
        else if (bytesReceived == 0)
        {
            cout << "Connection closed by peer\n";
            break;
        }
        else
        {
            // Append the received data to the message string
            receivedMessage.append(buffer, bytesReceived);

            // Check if the received message contains CRLF
            size_t crlfPos = receivedMessage.find("\r\n");
            if (crlfPos != string::npos)
            {
                // Found CRLF, process the message up to this point
                string messageUpToCRLF = receivedMessage.substr(0, crlfPos);
                cout << "Received message: " << messageUpToCRLF << endl;

                // Erase the processed part of the message from the string
                receivedMessage.clear();
            }
        }
    }

    if (receivedMessage[0] == 'C') // if message from coordinator
    {
        string msg = receivedMessage.substr(2);
        parse_coord_msg(msg);
    }
    else if (receivedMessage[0] == 'L') // message from load balancer
    {
        string msg = receivedMessage.substr(2);
        parse_lb_msg(msg);
    }
    else // error
    {
        logger.log("Message to admin console incorrectly formatted.", LOGGER_CRITICAL);
    }

    // Close the socket
    close(sockfd);
}

/*
Redirect to load balancer if user wants a different page
*/
void redirect_handler(const HttpRequest &req, HttpResponse &res)
{   
    int server = 7500;
    logger.log("Redirecting client to server with port " + server, LOGGER_INFO);
    // Format the server address properly assuming HTTP protocol and same host
    std::string redirectUrl = "http://localhost:" + server;
    res.set_code(303);
    //Set the response code to redirect and set the location header to the port of the frontend server it picked response.set_code(307);
    res.set_header("Location", redirectUrl); // Redirect to the server at the specified port response. set header ("Content-Type", "text/html");
    res.append_body_str("<html><body>Temporary Redirect to <a href='" + redirectUrl + "'›" + redirectUrl + "</a></body></html>");
}

int main()
{

    logger.log("Admin console init", LOGGER_INFO);
    /* Create sockets on defined ports 8000 and 8001 */

    // Create socket for port 8080
    int listen_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1)
    {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // Set socket option to enable address reuse
    int enable = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        std::cerr << "Setsockopt failed for port 8080\n";
        return 1;
    }

    string ip_addr = "127.0.0.1";
    int listen_port = 8080;
    struct sockaddr_in servaddr;
    socklen_t servaddr_size = sizeof(servaddr);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(listen_port);
    inet_aton(ip_addr.c_str(), &servaddr.sin_addr); // servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    // bind socket to port
    if (bind(listen_sock, (const struct sockaddr *)&servaddr, servaddr_size) == -1)
    {
        std::string msg = "Cannot bind socket to port #" + std::to_string(listen_port) + " (" + strerror(errno) + ")";
        logger.log(msg, LOGGER_CRITICAL);
        return 1;
    }

    // Listen for incoming connections
    if (listen(listen_sock, 3) < 0)
    {
        std::cerr << "Listen failed\n";
        return 1;
    }


    // Accept incoming connections and handle them in separate threads
    while (true)
    {
        int temp_sock = accept(listen_sock, (struct sockaddr *)&servaddr, &servaddr_size);
        if (temp_sock < 0)
        {
            std::cerr << "Accept failed for port 8080\n";
            return 1;
        }

        // spin up thread
        std::thread serv_thread(get_server_data, temp_sock);
        serv_thread.detach();

        // if we got messages from both coord and lb, exit loop
        if (lb_init && coord_init)
        {
            break;
        }
    }

    // // Initialize frontend (FE) servers
    // for (int i = 1; i <= 4; ++i)
    // {
    //     string fe_key = "FE" + to_string(i);
    //     int port_number = 8000 + i - 1; // Increment port number for each server
    //     lb_servers[fe_key] = port_number;
    //     server_status[fe_key] = 1; // Set status to 1 for each FE server
    // }

    // // Initialize Key-Value Store (KVS) servers
    // for (int i = 1; i <= 2; ++i)
    // { // Assuming two KVS servers
    //     string kvs_key = "KVS" + to_string(i);
    //     int port_number = 9000 + i - 1; // Increment port number for each server
    //     kvs_servers[kvs_key] = port_number;
    //     server_status[kvs_key] = 1; // Set status to 1 for each KVS server
    // }

    logger.log("Admin console ready.", LOGGER_INFO);


     // Create socket for port 8081
    int kvs_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (kvs_sock == -1)
    {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // Set socket option to enable address reuse
    if (setsockopt(kvs_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        std::cerr << "Setsockopt failed for port 8081\n";
        return 1;
    }

    int kvs_port = 8081;
    struct sockaddr_in kvs_sock_addr;
    socklen_t kvs_addr_len = sizeof(kvs_sock_addr);
    bzero(&kvs_sock_addr, sizeof(kvs_sock_addr));
    kvs_sock_addr.sin_family = AF_INET;
    kvs_sock_addr.sin_port = htons(kvs_port);
    inet_aton(ip_addr.c_str(), &kvs_sock_addr.sin_addr); // servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    // bind socket to port
    if (bind(kvs_sock, (const struct sockaddr *)&kvs_sock_addr, kvs_addr_len) == -1)
    {
        std::string msg = "Cannot bind socket to port #" + std::to_string(kvs_port) + " (" + strerror(errno) + ")";
        logger.log(msg, LOGGER_CRITICAL);
        return 1;
    }

    // Listen for incoming connections
    if (listen(kvs_sock, 3) < 0)
    {
        std::cerr << "Listen failed\n";
        return 1;
    }

    // Define Admin Console routes
    HttpServer::get("/admin/dashboard", dashboard_handler);                // Display admin dashboard
    HttpServer::post("/admin/component/toggle", toggle_component_handler); // Handle component toggle requests

    // @todo add redirect to LB
    HttpServer::get("*", redirect_handler);

    // Run Admin HTTP server
    HttpServer::run(8080); // Assuming port 8080 for Admin Console

    return 0;
}