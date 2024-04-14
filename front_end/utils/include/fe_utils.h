#ifndef FE_UTILS_H
#define FE_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include "../../../http_server/include/http_request.h"

// #include "../../../utils/include/utils.h"
// #include "../../include/main.h"

const std::string SERVADDR = "127.0.0.1";
const int SERVPORT = 8000;

// Helper function to appends args to vector
// current format: COMMAND<SP><SP>arg1<SP><SP>arg2.....
void insert_arg(std::vector<char> &curr_vec, std::vector<char> arg);

// Helper function for all writes to kvs
size_t writeto_kvs(const std::vector<char> &data, int fd);

// Helper function for all reads from kvs responses
std::vector<char> readfrom_kvs(int fd);

namespace FeUtils
{
    // creates socket, connects to kvs server and returns fd
    // @note: don't need s_addr and s_port for now, but can have default and overloaded value for auth?
    int open_socket(const std::string s_addr = SERVADDR, const int s_port = SERVPORT);

    // helper function that, given a path, queries coordinator for corresponding
    // KVS server address and returns it
    std::vector<std::string> query_coordinator(std::string& path);

    // pass a fd and row, col values to perform GET(r,c), returns value
    std::vector<char> kv_get(int fd, std::vector<char> row, std::vector<char> col);

    // pass a fd and row to get all column values of row "RGET(r)"
    std::vector<char> kv_get_row(int fd, std::vector<char> row);

    // pass a fd and row, col, value to perform PUT(r,c,v)
    std::vector<char> kv_put(int fd, std::vector<char> row, std::vector<char> col, std::vector<char> val);

    // pass a fd and row, col, value1, value2 to perform CPUT(r,c,v1, v2)
    std::vector<char> kv_cput(int fd, std::vector<char> row, std::vector<char> col, std::vector<char> val1, std::vector<char> val2);

    // pass a fd and row, col to perform DELETE(r,c)
    std::vector<char> kv_del(int fd, std::vector<char> row, std::vector<char> col);

    // checks if a char vector starts with +OK
    bool kv_success(const std::vector<char> &vec);

    // helper function that parsing cookie header responses from request objects
    std::unordered_map<std::string, std::string> parse_cookies(std::vector<std::string> &cookies_vector);

    // helper function that resets cookies in route handler
    void set_cookies(HttpResponse res, std::string username, std::string sid);

    // helper function that validates sessionID of a user
    // returns empty string if session ID is invalid, otherwise returns SID
    std::string validate_session_id(int kvs_fd, std::string &username, const HttpRequest &req);
}

#endif