#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "http_response.h"

struct HttpRequest {
    // ensures request is reset to default values when initialized
    HttpRequest() {
        reset();
    }

    public:
        // Request line
        std::string method;             // HTTP method (GET, POST, ...)
        std::string path;               // request URL
        std::string version;            // HTTP version (HTTP/1.1)

    private:
        // Request metadata
        bool is_static;
        std::string static_resource_path;
        std::function<void(const HttpRequest&, HttpResponse&)> dynamic_route;

        std::unordered_map<std::string, std::vector<std::string>> headers;
        std::vector<char> body; // store data directly as bytes

        // reset data fields after transaction is complete - for internal http server use only
        void reset() {
            method.clear();
            path.clear();
            version.clear();
            headers.clear();
            body.clear();
            is_static = true;
            static_resource_path.clear();
            dynamic_route = nullptr;
        }

        friend class Client;
    public:
        // get a vector of header values for a header
        // Note that this returns a vector because a header is allowed to have multiple associated values
        std::vector<std::string> get_header(const std::string& header) {
            if (headers.count(header) == 0) {
                std::vector<std::string> empty;
                return empty;
            }
            return headers[header];
        }

        // returns the response body represented as a string
        std::string body_as_string() {
            return std::string(body.data(), body.size());
        }

        // returns the response body represented as bytes
        std::vector<char> body_as_bytes() {
            return body;
        }
};

#endif