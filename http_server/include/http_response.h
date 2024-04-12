#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>
#include <vector>

#include "http_server.h"
#include "client.h"

struct HttpResponse {
    // ensures response is reset to default values when initialized
    HttpResponse() {
        reset();
    }

    private:
        // Headers
        std::unordered_map<std::string, std::vector<std::string>> headers;

        // Body
        std::vector<char> body; // store data directly as bytes

        std::string version;
        int code;
        std::string reason;

        friend class Client;
    public:
        void set_header(const std::string& header, const std::string& value) {
            headers[header].push_back(value);
        }

        void set_code(int res_code) {
            code = res_code;
            reason = HttpServer::response_codes.at(res_code);
        }

        void set_cookie(std::string& key, std::string& value) {
            set_header("Set-Cookie", key + "=" + value);
            // cookie expires after 20 minutes (1200 seconds)
            set_header("Set-Cookie", key + "=" + value + "; Max-Age=1200");
        }

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

        // reset data fields (headers and body)
        void reset() {
            version = "HTTP/1.1";
            code = 0;
            reason.clear();
            headers.clear();
            body.clear();
        }
};

#endif