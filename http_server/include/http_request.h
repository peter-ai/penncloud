#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <vector>

struct HttpRequest {
    // Request line
    std::string req_method;         // HTTP method (GET, POST, ...)
    std::string path;               // request URL
    std::string version;            // HTTP version (HTTP/1.1)

    // Headers
    std::unordered_map<std::string, std::vector<std::string>> headers;

    // Body
    std::vector<char> body; // store data directly as bytes

    // Metadata
    bool is_static = true;
    std::string static_resource_path;

    // TODO add field for session

    // clear data fields after transaction is complete
    void clear() {
        headers.clear();
        body.clear();
    }
};

#endif