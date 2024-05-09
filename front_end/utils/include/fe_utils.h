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
#include <sstream>
#include <iomanip>
#include <sstream>
#include "../../../http_server/include/http_request.h"
#include "../../../utils/include/utils.h"
#include "../../include/email_data.h"

const std::string SERVADDR = "127.0.0.1";
const int SERVPORT = 6000;

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

    // gets all rows from a given server
    std::vector<char> kvs_get_allrows(int fd);

    // takes in the vector of all rows coming from the server and separates into a vecotr of strings
    std::vector<std::string> parse_all_rows(std::vector<char> &tablet);

    // pass a fd and row to perform DELETEROW(r)
    std::vector<char> kv_del_row(int fd, std::vector<char> row);

    // pass a fd, old rowname and new row name to perform RENAME(r1, r2)
    std::vector<char> kv_rename_row(int fd, std::vector<char> oldrow, std::vector<char> newrow);

    // pass a fd, row, old col name and new col name to perform RENAME(r, c1, c2)
    std::vector<char> kv_rename_col(int fd, std::vector<char> row, std::vector<char> oldcol, std::vector<char> newcol);

    // checks if a char vector starts with +OK
    bool kv_success(const std::vector<char> &vec);

    /// @brief helper function that parses cookie header responses received in http requests
    /// @param cookies_vector a vector containing cookies of the form <"key1=value1", "key2=value2", "key3=value3", ...>
    /// @return a map with keys and values from the cookie
    std::unordered_map<std::string, std::string> parse_cookies(const HttpRequest &req);

    /// @brief sets the cookies on the http response and resets the age of cookies
    /// @param res HttpResponse object
    /// @param username username associated with the current session
    /// @param sid session ID associated with the current session
    void set_cookies(HttpResponse &res, std::string username, std::string sid);

    /// @brief expires the cookies on the http response by setting age to 0
    /// @param res HttpResponse object
    /// @param username username associated with the current session
    /// @param sid session ID associated with the current session
    void expire_cookies(HttpResponse &res, std::string username, std::string sid);

    /// @brief validates the session id for the current user
    /// @param kvs_fd file descriptor for KVS server
    /// @param username username associatd with the current session to be validated
    /// @param req HttpRequest object
    /// @return returns an empty string if the session is invalid, otherwise returns the user's session ID
    std::string validate_session_id(int kvs_fd, std::string &username, const HttpRequest &req);

    std::vector<std::vector<char>> split_vector(const std::vector<char> &data, const std::vector<char> &delimiter);

    std::string urlEncode(const std::string value);

    std::string urlDecode(const std::string value);

    // ### MAILBOX/SMTPSERVER UTILS ###
    std::string extractUsernameFromEmailAddress(const std::string &emailAddress);

    std::vector<std::string> parseRecipients(const std::string &recipients);

    std::vector<char> charifyEmailContent(const EmailData &email);

    bool startsWith(const std::vector<char> &vec, const std::string &prefix);

    std::vector<char> charifyEmailContent(const EmailData &email);
    
    // Relay specific
    std::string extractDomain(const std::string &email);

    bool isLocalDomain(const std::string &domain);

}

#endif