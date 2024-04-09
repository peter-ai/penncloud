#include "http_server/include/http_server.h"

int main()
{
    HttpServer::get("/test.txt");
    HttpServer::run(8000);
    return(0);
}