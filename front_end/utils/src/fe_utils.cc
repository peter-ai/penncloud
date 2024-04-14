#include "../include/fe_utils.h"

// Helper function to appends args to vector
// current format: COMMAND<SP><SP>arg1<SP><SP>arg2.....
void insert_arg(std::vector<char> &curr_vec, std::vector<char> arg)
{
    // can modify depending on delimiter of chouce
    curr_vec.push_back(' ');
    curr_vec.push_back(' ');
    curr_vec.insert(curr_vec.end(), arg.begin(), arg.end());
}

// Helper function for all writes to kvs
size_t writeto_kvs(const std::vector<char> &data, int fd)
{
    // Send data to kvs using fd
    ssize_t bytes_sent = send(fd, data.data(), data.size(), 0);
    if (bytes_sent == -1)
    {
        std::cerr << "Error sending data" << std::endl;
        return 0;
    }

    // std::cout << "Sent message: ";
    // for (char c : data)
    // {
    //     std::cout << c;
    // }
    // std::cout << std::endl;

    return bytes_sent;
}

// Helper function for all reads from kvs responses
std::vector<char> readfrom_kvs(int fd)
{
    std::string stringBuffer(1024, '\0'); // Start with a large initial size like 1MB
    size_t current_capacity = stringBuffer.size();
    size_t total_bytes_received = 0;
    ssize_t bytes_received = 0;

    // Prepare the poll structure
    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLIN; // Check for data to read

    int timeout = 100; // Timeout in milliseconds (1 second)

    do
    {
        // Wait for the socket to be ready for reading using poll
        int retval = poll(fds, 1, timeout);

        if (retval == -1)
        {
            std::cerr << "Poll error: " << strerror(errno) << std::endl;
            return std::vector<char>{'-', 'E', 'R'};
        }
        else if (retval)
        {
            // Check if we have events on the socket
            if (fds[0].revents & POLLIN)
            {
                // Data is available to read
                bytes_received = recv(fd, &stringBuffer[total_bytes_received], current_capacity - total_bytes_received, 0);

                if (bytes_received == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // Non-blocking mode, no data available at the moment
                        continue;
                    }
                    else
                    {
                        // An actual error occurred
                        std::cerr << "Error receiving data: " << strerror(errno) << std::endl;
                        return std::vector<char>{'-', 'E', 'R'};
                    }
                }
                else if (bytes_received == 0)
                {
                    // The connection has been closed by the peer
                    std::cerr << "Connection closed by peer" << std::endl;
                    break;
                }

                total_bytes_received += bytes_received;
                // std::cout << "Received " << bytes_received << " bytes, Total: " << total_bytes_received << " bytes." << std::endl;

                // Ensure there is enough room to receive more data
                if (total_bytes_received == current_capacity)
                {
                    current_capacity *= 2; // Double the capacity
                    stringBuffer.resize(current_capacity);
                }
            }
        }
        else
        {
            // No data within the timeout period
            break;
        }
    } while (true);

    stringBuffer.resize(total_bytes_received); // Resize to fit actual data received
    return std::vector<char>(stringBuffer.begin(), stringBuffer.begin() + total_bytes_received);
}

// Opens socket and sends parameters
int FeUtils::open_socket(const std::string s_addr, const int s_port)
{

    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        //@todo: potentially log instead
        std::cerr << "Error creating socket" << std::endl;
        return -1;
    }

    // struct set up for server addr
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(s_port);
    inet_pton(AF_INET, s_addr.c_str(), &server_addr.sin_addr);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        std::cerr << "Error connecting to server" << std::endl;
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/// @brief helper function that queries coordinator for KVS server address given a path
/// @param path file/folder path for which the associated KVS server address should be retrieved
/// @return returns the server address as a vector of strings <ip,port>
std::vector<std::string> FeUtils::query_coordinator(std::string &path)
{
    // set ip:port for coordinator
    std::string ip_addr = "127.0.0.1";
    int port = 4999;

    // open socket for coordinator
    int coord_sock = FeUtils::open_socket(ip_addr, port);

    // set buffer
    std::string resp;
    resp.resize(100);
    int rlen = 0;

    // send request and retrieve response
    send(coord_sock, &path[0], path.size(), 0);
    rlen = recv(coord_sock, &resp[0], resp.size(), 0);
    resp.resize(rlen);
    std::vector<std::string> kvs_addr = Utils::split(resp, ":");

    // close socket for coordinator
    close(coord_sock);

    return kvs_addr;
}

// Function for KV GET(row, col). Returns value as vector<char> to user
std::vector<char> FeUtils::kv_get(int fd, std::vector<char> row, std::vector<char> col)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "GET";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, col);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// Puts a row, col, value into kvs using PUT(r,c,v), returns 0 or 1 as status
std::vector<char> FeUtils::kv_put(int fd, std::vector<char> row, std::vector<char> col, std::vector<char> val)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "PUT";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, col);
    insert_arg(fn_string, val);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// Gets a row's  columns using RGET(r), returns list of cols
std::vector<char> FeUtils::kv_get_row(int fd, std::vector<char> row)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "RGET";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// Puts a row, col, value into kvs using CPUT(r,c,v1,v2), returns 0 or 1 as status
std::vector<char> FeUtils::kv_cput(int fd, std::vector<char> row, std::vector<char> col, std::vector<char> val1, std::vector<char> val2)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "CPUT";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, col);
    insert_arg(fn_string, val1);
    insert_arg(fn_string, val2);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// Deletes a row, col, value into kvs using DEL(r,c), returns 0 or 1 as status
std::vector<char> FeUtils::kv_del(int fd, std::vector<char> row, std::vector<char> col)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "DELETE";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, col);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// returns true is +OK at the start, else returns false
bool FeUtils::kv_success(const std::vector<char> &vec)
{
    // Check if the vector has at least 3 characters
    if (vec.size() < 3)
    {
        return false;
    }

    // Define the expected prefix
    std::vector<char> prefix = {'+', 'O', 'K'};

    // Check if the first three characters match the prefix
    return std::equal(prefix.begin(), prefix.end(), vec.begin());
}

/// @brief helper function that parses cookie header responses received in http requests
/// @param cookies_vector a vector containing cookies of the form <"key1=value1", "key2=value2", "key3=value3", ...>
/// @return a map with keys and values from the cookie
std::unordered_map<std::string, std::string> FeUtils::parse_cookies(std::vector<std::string> &cookie_vector)
{
    std::unordered_map<std::string, std::string> cookies;
    for (unsigned long i = 0; i < cookie_vector.size(); i++)
    {
        std::vector<std::string> cookie = Utils::split(cookie_vector[i], "=");
        cookies[Utils::trim(cookie[0])] = Utils::trim(cookie[1]);
    }

    return cookies;
}

/// @brief sets the cookies on the http response and resets the age of cookies
/// @param res HttpResponse object
/// @param username username associated with the current session
/// @param sid session ID associated with the current session
void FeUtils::set_cookies(HttpResponse &res, std::string username, std::string sid)
{
    const std::string key1 = "user";
    const std::string key2 = "sid";
    res.set_cookie(key1, username);
    res.set_cookie(key2, sid);
}

/// @brief validates the session id of the current user
/// @param kvs_fd file descriptor for KVS server
/// @param username username associatd with the current session to be validated
/// @param req HttpRequest object
/// @return returns an empty string if the session is invalid, otherwise returns the user's session ID
std::string FeUtils::validate_session_id(int kvs_fd, std::string &username, const HttpRequest &req)
{
    // get sid from KVS
    std::vector<char> row_key(username.begin(), username.end());
    row_key.push_back('/');
    std::string c_key_str = "sid";
    std::vector<char> col_key(c_key_str.begin(), c_key_str.end());
    const std::vector<char> kvs_sid = FeUtils::kv_get(kvs_fd, row_key, col_key); // TODO: RETRY
    std::string sid;

    // check if response from KVS is OK
    if (FeUtils::kv_success(kvs_sid))
    {
        sid.assign(kvs_sid.begin() + 4, kvs_sid.end());
    }
    else
    {
        return ""; // TODO: RETRY ??
    }

    // if sid is empty, no active session
    if (sid.empty())
    {
        return "";
    }

    // parse cookies sent with request
    std::vector<std::string> cookie_vector = req.get_header("Cookie");
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(cookie_vector);

    // check if cookie for sid was sent with request
    if (cookies.count(c_key_str))
    {
        // if cookie matches the KVS, there is a valid session for the client
        if (cookies[c_key_str].compare(sid) == 0)
            return sid;
        else
            return "";
    }
    else
    {
        return "";
    }
}