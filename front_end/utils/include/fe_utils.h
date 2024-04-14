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
#include <iostream>
#include <algorithm> // std::transform
#include <sys/time.h>
#include <poll.h> // For poll
#include "../../../http_server/include/http_request.h"

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

    /// @brief helper function that queries coordinator for KVS server address given a path
    /// @param path file/folder path for which the associated KVS server address should be retrieved
    /// @return returns the server address as a vector of strings <ip,port>
    std::vector<std::string> query_coordinator(std::string &path);

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

    /// @brief helper function that parses cookie header responses received in http requests
    /// @param cookies_vector a vector containing cookies of the form <"key1=value1", "key2=value2", "key3=value3", ...>
    /// @return a map with keys and values from the cookie
    std::unordered_map<std::string, std::string> parse_cookies(const HttpRequest& req);

    /// @brief sets the cookies on the http response and resets the age of cookies
    /// @param res HttpResponse object
    /// @param username username associated with the current session
    /// @param sid session ID associated with the current session
    void set_cookies(HttpResponse &res, std::string username, std::string sid);

    /// @brief validates the session id for the current user
    /// @param kvs_fd file descriptor for KVS server
    /// @param username username associatd with the current session to be validated
    /// @param req HttpRequest object
    /// @return returns an empty string if the session is invalid, otherwise returns the user's session ID
    std::string validate_session_id(int kvs_fd, std::string &username, const HttpRequest &req);
}

#endif