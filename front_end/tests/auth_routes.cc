#include "../include/authentication.h"

int main()
{
    // register signup route
    // auto signup_route = std::bind(signup_handler, std::placeholders::_1, std::placeholders::_2);
    // HttpServer::post("/api/signup", signup_route);

    // register login route
    auto login_route = std::bind(login_handler, std::placeholders::_1, std::placeholders::_2);
    HttpServer::post("/api/login", login_route);

    // register logout route
    // auto logout_route = std::bind(logout_handler, std::placeholders::_1, std::placeholders::_2);
    // HttpServer::post("/api/logout", logout_route);

    // register password update route
    // auto update_pass_route = std::bind(update_password_handler, std::placeholders::_1, std::placeholders::_2);
    // HttpServer::post("/api/update_pass", update_pass_route);

    HttpServer::run(8000);
    return (0);
}