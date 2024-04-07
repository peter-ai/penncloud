#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "http_server.h"

struct HttpResponse {
    // Response line
    std::string version = HttpServer::version;         
    int code;
    std::string reason;            

    // Headers
    std::unordered_map<std::string, std::vector<std::string>> headers;

    // Body
    std::string body;
};

#endif