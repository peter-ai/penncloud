#include "../include/authentication.h"


int main()
{
    auto login_route = std::bind(login_handler, std::placeholders::_1, std::placeholders::_2);
    HttpServer::post("/api/login", login_route);

    HttpServer::run(8000);
    return(0);
}