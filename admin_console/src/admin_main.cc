/*
 * admin_main.cc
 *
 *  Created on: Apr 25, 2024
 *      Author: aashok12
 *
 * Runs admin console functionality on an HTTP server
 *
 */

#include "../../http_server/include/http_server.h"
#include "../../front_end/utils/include/fe_utils.h"

using namespace std;
// Dummy global variables for load balancer and coordinator
std::string loadBalancerIP = "127.0.0.1";
int loadBalancerPort = 8081;
std::string coordinatorIP = "127.0.0.1";
int coordinatorPort = 8082;

// // Function to contact the load balancer and coordinator
// std::string contactServers() {
//     // Contact the load balancer
//     std::stringstream lbStream;
//     lbStream << "fe1,fe2,fe3\r\n"; // Dummy data for frontend nodes
//     std::string feData = lbStream.str();

//     // Contact the coordinator
//     std::stringstream coordinatorStream;
//     coordinatorStream << "kvs1,kvs2,kvs3\r\n"; // Dummy data for KVS components
//     std::string kvsData = coordinatorStream.str();

//     // Combine the data
//     std::string combinedData = feData + kvsData;

//     return combinedData;
// }

void dashboard_handler(const HttpRequest &req, HttpResponse &res)
{
    std::string user = "example"; // Extract user information from cookies if needed

    //   std::string page =
    //     "<!doctype html>"
    //     "<html lang='en'>"
    //     "<head>"
    //     "<meta charset='utf-8'>"
    //     "<meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'>"
    //     "<title>Admin Dashboard</title>"
    //     "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'>"
    //     "<style>"
    //     ".switch { position: relative; display: inline-block; width: 30px; height: 15px; }"
    //     ".switch input { opacity: 0; width: 0; height: 0; }"
    //     ".slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; }"
    //     ".slider:before { position: absolute; content: ''; height: 11px; width: 11px; left: 2px; bottom: 2px; background-color: white; transition: .4s; }"
    //     "input:checked + .slider { background-color: #2196F3; }"
    //     "input:focus + .slider { box-shadow: 0 0 1px #2196F3; }"
    //     "input:checked + .slider:before { transform: translateX(15px); }"
    //     ".slider.round { border-radius: 34px; }"
    //     ".slider.round:before { border-radius: 50%; }"
    //     "</style>"
    //     "</head>"
    //     "<body>"
    //     "<div class='container mt-4'>"
    //     "<h1 class='mb-4'>Admin Dashboard</h1>";

    // // Dark/Light Mode Toggle
    // page += "<h2>Theme</h2>";
    // page += "<div class='form-check form-switch'>";
    // page += "<label class='form-check-label' for='darkmode'>Dark Mode</label>";
    // page += "<label class='switch'>";
    // page += "<input type='checkbox' id='darkmode'>";
    // page += "<span class='slider round'></span>";
    // page += "</label>";
    // page += "</div>";

    // // Frontend Nodes
    // page += "<div class='row'>";
    // page += "<div class='col'>";
    // page += "<h2>Frontend Nodes</h2>";
    // for (int i = 1; i <= 3; ++i) {
    //     page += "<div class='form-check form-switch'>";
    //     page += "<label class='form-check-label' for='fe" + std::to_string(i) + "'>FE" + std::to_string(i) + "</label>";
    //     page += "<label class='switch'>";
    //     page += "<input type='checkbox' id='fe" + std::to_string(i) + "'>";
    //     page += "<span class='slider round'></span>";
    //     page += "</label>";
    //     page += "</div>";
    // }
    // page += "</div>";

    // // Key-Value Store Components
    // page += "<div class='col'>";
    // page += "<h2>Key-Value Store Components</h2>";
    // for (int i = 1; i <= 3; ++i) {
    //     page += "<div class='form-check form-switch'>";
    //     page += "<label class='form-check-label' for='kvs" + std::to_string(i) + "'>KVS" + std::to_string(i) + "</label>";
    //     page += "<label class='switch'>";
    //     page += "<input type='checkbox' id='kvs" + std::to_string(i) + "'>";
    //     page += "<span class='slider round'></span>";
    //     page += "</label>";
    //     page += "</div>";
    // }
    // page += "</div>";
    // page += "</div>";

    // // Paginated Table
    // page += "<div class='mt-4'><h2>Table</h2>";
    // page += "<table class='table table-bordered'>";
    // page += "<thead><tr><th>ID</th><th>Name</th><th>Value</th></tr></thead>";
    // page += "<tbody>";
    // for (int i = 1; i <= 10; ++i) {
    //     page += "<tr><td>" + std::to_string(i) + "</td><td>Component</td><td>Value</td></tr>";
    // }
    // page += "</tbody>";
    // page += "</table>";

    // // Pagination
    // page += "<nav><ul class='pagination justify-content-center'>";
    // page += "<li class='page-item'><a class='page-link' href='#'>Previous</a></li>";
    // page += "<li class='page-item'><a class='page-link' href='#'>1</a></li>";
    // page += "<li class='page-item'><a class='page-link' href='#'>2</a></li>";
    // page += "<li class='page-item'><a class='page-link' href='#'>3</a></li>";
    // page += "<li class='page-item'><a class='page-link' href='#'>Next</a></li>";
    // page += "</ul></nav>";
    // page += "</div>";

    // page += "</div>"
    //         "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'></script>"
    //         "</body>"
    //         "</html>";

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
        "<h2>Theme</h2>"
        "<div class='form-check form-switch'>"
        "<label class='form-check-label' for='darkmode'>Dark Mode</label>"
        "<label class='switch'>"
        "<input type='checkbox' id='darkmode'>"
        "<span class='slider round'></span>"
        "</label>"
        "</div>"
        "<div class='row'>";

    // Frontend Nodes
    page += "<div class='row'>";
    page += "<div class='col'>";
    page += "<h2>Frontend Nodes</h2>";
    for (int i = 1; i <= 3; ++i) {
        page += "<div class='form-check form-switch'>";
        page += "<label class='form-check-label' for='fe" + std::to_string(i) + "'>FE" + std::to_string(i) + "</label>";
        page += "<label class='switch'>";
        page += "<input type='checkbox' id='fe" + std::to_string(i) + "'>";
        page += "<span class='slider round'></span>";
        page += "</label>";
        page += "</div>";
    }
    page += "</div>";

    // Key-Value Store Components
    page += "<div class='col'>";
    page += "<h2>Key-Value Store Components</h2>";
    for (int i = 1; i <= 3; ++i) {
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
    for (int i = 1; i <= 10; ++i) {
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
            "</script>"
            "</body>"
            "</html>";


    res.set_code(200);
    res.append_body_str(page);
    res.set_header("Content-Type", "text/html");
}

int main()
{
    // Define Admin Console routes
    HttpServer::get("/admin/dashboard", dashboard_handler); // Display admin dashboard
    // HttpServer::post("/admin/component/toggle", toggle_component_handler); // Handle component toggle requests

    // Run Admin HTTP server
    HttpServer::run(8080); // Assuming port 8080 for Admin Console

    return 0;
}