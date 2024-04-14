#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>

struct HttpResponse
{
    // ensures response is reset to default values when initialized
    HttpResponse()
    {
        reset();
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> headers;
    std::vector<char> body; // store data directly as bytes
    std::string version;
    int code;
    std::string reason;

    // reset data fields - for internal http server use only
    void reset()
    {
        version = "HTTP/1.1";
        code = 0;
        reason.clear();
        headers.clear();
        body.clear();
    }

    friend class Client;

public:

    // setsheader and corresponding value in response
    // Please note that it's the responsibility of the route handler to ensure header names and values are set correctly
    void set_header(const std::string& header, const std::string& value) {
        headers[header].push_back(value);
    }

    // sets response code
    // ! Note that if a response code you need is NOT included here, please let me know or push a hotfix that adds the code and its reason to the map below
    void set_code(int res_code)
    {
        code = res_code;
        const std::unordered_map<int, std::string> response_codes = {
            {200, "OK"},
            {400, "Bad Request"},  // request is not as the API expects
            {401, "Unauthorized"}, // no credentials or invalid credentials
            {403, "Forbidden"},    // valid credentials but not enough privileges to perform an action on a resource
            {404, "Not Found"},
            {405, "Method Not Allowed"},
            {501, "Not Implemented"},
            {505, "HTTP Version Not Supported"}};
        reason = response_codes.at(res_code);
    }

    // sets cookie in response
    // Note that cookies have a default expiry of 20 minutes (hard coded for convenience)
    void set_cookie(const std::string& key, const std::string& value) {
        set_header("Set-Cookie", key + "=" + value + "; Max-Age=1200; HttpOnly");
    }

    // append binary data to body - useful if you're writing the contents of a file to body since file may contain /0
    void append_body_bytes(const char *bytes, std::size_t size)
    {
        body.insert(body.end(), bytes, bytes + size);
    }

    // append string data to body
    void append_body_str(const std::string &s)
    {
        for (char c : s)
        {
            body.push_back(c);
        }
    }

    // get the size of the body - you can call this when setting the content length header in your response
    std::string body_size()
    {
        return std::to_string(body.size());
    }
};

#endif