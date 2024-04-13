#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "http_response.h"
#include "../../utils/include/utils.h"

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
        std::unordered_map<std::string, std::string> query_params;

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
        std::vector<std::string> get_header(const std::string& header) const {
            std::string header_name = Utils::to_lowercase(header);
            if (headers.count(header_name) == 0) {
                std::vector<std::string> empty;
                return empty;
            }
            
            // process depending on whether cookies were requested or not
            if (header_name.compare("cookie") != 0) return headers.at(header_name);
            else
            {
                std::string cookie_str = headers.at(header_name)[0];
                std::vector<std::string> cookies = Utils::split(cookie_str, "; ");
                return cookies;
            }
        }

        // returns the response body represented as a string
        std::string body_as_string() const {
            return std::string(body.data(), body.size());
        }

        // returns the response body represented as bytes
        std::vector<char> body_as_bytes() const {
            return body;
        }
};

#endif