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

    void set_header(const std::string header, const std::string& value) {
        headers[header].push_back(value);
    }

    // Body
    std::vector<char> body; // store data directly as bytes

    // append binary date to body - useful if you're writing the contents of a file to body since file may contain /0
    void append_body_bytes(const char* bytes, std::size_t size) {
        body.insert(body.end(), bytes, bytes + size);
    }

    // append string to body
    void append_body_str(const std::string& s) {
        for (char c : s) {
            body.push_back(c);
        }
    }
};

#endif