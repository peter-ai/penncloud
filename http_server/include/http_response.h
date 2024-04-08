#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>
#include <vector>

struct HttpResponse {
    // Response line
    std::string version = "HTTP/1.1";
    int code;
    std::string reason;            

    // Headers
    std::unordered_map<std::string, std::vector<std::string>> headers;

    // Body
    std::vector<char> body; // store data directly as bytes
};

#endif