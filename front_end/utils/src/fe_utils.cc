/*
 *   Helper functions for front-end route handlers.
 *   Utility functions that assist in communication with the KVS.
 */

#include "../include/fe_utils.h"

Logger fe_utils_logger("FE Utils");

// Helper function to appends args to vector
// current format: COMMAND<SP><SP>arg1<SP><SP>arg2.....
void insert_arg(std::vector<char> &curr_vec, std::vector<char> arg)
{
    // can modify depending on delimiter of chouce
    curr_vec.push_back('\b');
    curr_vec.insert(curr_vec.end(), arg.begin(), arg.end());
}

// Helper function for all writes to kvs
size_t writeto_kvs(std::vector<char> &msg, int fd)
{
    // Send data to kvs using fd
    uint32_t msg_size = htonl(msg.size());

    std::vector<uint8_t> size_prefix(sizeof(uint32_t));
    // Copy bytes from msg_size into the size_prefix vector
    std::memcpy(size_prefix.data(), &msg_size, sizeof(uint32_t));

    // Insert the size prefix at the beginning of the original response msg vector
    msg.insert(msg.begin(), size_prefix.begin(), size_prefix.end());

    // write response to client as bytes
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < msg.size())
    {
        int bytes_sent = send(fd, msg.data() + total_bytes_sent, msg.size() - total_bytes_sent, 0);
        total_bytes_sent += bytes_sent;
    }

    // logging message
    fe_utils_logger.log("Message Sent to KVS (" + std::to_string(total_bytes_sent) + " bytes) - " + std::string(msg.begin(), msg.end()), LOGGER_INFO);

    return total_bytes_sent;
}

// Helper function for all reads from kvs responses
std::vector<char> readfrom_kvs(int fd)
{
    std::vector<char> kvs_data;
    char buffer[4096];
    uint32_t data_size = 0;
    size_t total_bytes_recvd = 0;
    bool size_extracted = false;
    char size_buffer[4];
    int size_buffer_filled = 0;

    while (true)
    {
        int bytes_recvd = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes_recvd < 0)
        {
            // Log error and exit on read error
            fe_utils_logger.log("Error reading from KVS server", 40);
            break;
        }
        else if (bytes_recvd == 0)
        {
            // Log and exit when KVS server closes the connection
            fe_utils_logger.log("KVS server closed connection", 30);
            break;
        }

        int buffer_offset = 0;

        // Collect bytes for the data size if not already extracted
        if (!size_extracted)
        {
            while (size_buffer_filled < 4 && buffer_offset < bytes_recvd)
            {
                size_buffer[size_buffer_filled++] = buffer[buffer_offset++];
            }

            // Check if we have collected 4 bytes for the data size
            if (size_buffer_filled == 4)
            {
                memcpy(&data_size, size_buffer, 4);
                data_size = ntohl(data_size); // Convert from network byte order to host byte order
                size_extracted = true;
            }
        }

        // If data size is known, collect the KVS data
        if (size_extracted)
        {
            int remaining_data = bytes_recvd - buffer_offset;
            if (remaining_data > 0)
            {
                int needed_data = data_size - total_bytes_recvd;
                int data_to_copy = std::min(remaining_data, needed_data);
                kvs_data.insert(kvs_data.end(), buffer + buffer_offset, buffer + buffer_offset + data_to_copy);
                total_bytes_recvd += data_to_copy;
            }
        }

        // Exit the loop if the entire data set has been received
        if (total_bytes_recvd == data_size && size_extracted)
        {
            break;
        }
    }

    return kvs_data;
}

// Opens socket and sends parameters
int FeUtils::open_socket(const std::string s_addr, const int s_port)
{

    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        //@todo: potentially log instead
        fe_utils_logger.log("Error creating socket", 40);
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
        fe_utils_logger.log("Error connecting to server", 40);
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
    // string to send  COMMAND + \b + row + \b + col....
    std::string cmd = "GETV";
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
    std::string cmd = "PUTV";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, col);
    insert_arg(fn_string, val);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        fe_utils_logger.log("Unable to write to KVS server", 40);
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// Gets a row's  columns using GETR(r), returns list of cols
std::vector<char> FeUtils::kv_get_row(int fd, std::vector<char> row)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "GETR";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    std::vector<char> response = {};

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        fe_utils_logger.log("Unable to write to KVS server", 40);
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
        fe_utils_logger.log("Unable to write to KVS server", 40);
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
    std::string cmd = "DELV";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, col);
    std::vector<char> response = {};

    fe_utils_logger.log("Sending message:" + std::string(fn_string.begin(), fn_string.end()), LOGGER_DEBUG);

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        fe_utils_logger.log("Unable to write to KVS server", 40);
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
std::unordered_map<std::string, std::string> FeUtils::parse_cookies(const HttpRequest &req)
{
    std::vector<std::string> cookie_vector = req.get_header("Cookie");
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

/// @brief expires the cookies on the http response by setting age to 0
/// @param res HttpResponse object
/// @param username username associated with the current session
/// @param sid session ID associated with the current session
void FeUtils::expire_cookies(HttpResponse &res, std::string username, std::string sid)
{
    const std::string key1 = "user";
    const std::string key2 = "sid";
    res.set_cookie(key1, username, "0");
    res.set_cookie(key2, sid, "0");
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
    std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(req);

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

// Function for KV GET(row, col). Returns value as vector<char> to user
std::vector<char> FeUtils::kvs_get_allrows(int fd)
{
    // string to send  COMMAND + \b + row + \b + col....
    std::string cmd = "GETA";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
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

// Function to split a vector<char> based on a vector<char> delimiter
std::vector<std::vector<char>> FeUtils::split_vector(const std::vector<char> &data, const std::vector<char> &delimiter)
{
    std::vector<std::vector<char>> result;
    size_t start = 0;
    size_t end = data.size();

    if (data.size() == 0)
    {
        return {{}};
    }

    while (start < end)
    {
        // Find the next occurrence of delimiter starting from 'start'
        auto pos = search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

        if (pos == data.end())
        {
            // No delimiter found, copy the rest of the vector
            result.emplace_back(data.begin() + start, data.end());
            break;
        }
        else
        {
            // Delimiter found, copy up to the delimiter and move 'start' past the delimiter
            result.emplace_back(data.begin() + start, pos);
            start = distance(data.begin(), pos) + delimiter.size();
        }
    }

    return result;
}

// takes in the vector of all rows coming from the server and separates into a vecotr of strings
std::vector<std::string> FeUtils::parse_all_rows(std::vector<char> &tablet)
{
    std::vector<std::string> rows;
    if (tablet.at(0) == '+' && tablet.at(1) == 'O' && tablet.at(2) == 'K')
    {

        // strip +OK<sp>
        tablet.erase(tablet.begin(), tablet.begin() + 4);

        // if tablet is now empty, i.e. no rows, reutrn
        if (tablet.empty())
        {
            return rows;
        }

        // otherwise loop thru and aggregate until we get each row spearated by \b
        std::vector<std::vector<char>> split_rows = split_vector(tablet, {'\b'});

        for (auto vec : split_rows)
        {
            if (!vec.empty())
            {
                std::string rowname(vec.begin(), vec.end());
                rows.push_back(rowname);
            }
        }
    }

    return rows;
}

// pass a fd and row to perform DELETEROW(r)
std::vector<char> FeUtils::kv_del_row(int fd, std::vector<char> row)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "DELR";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    fn_string.push_back('\b');
    std::vector<char> response = {};

    fe_utils_logger.log("Sending message:" + std::string(fn_string.begin(), fn_string.end()), LOGGER_DEBUG);

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        fe_utils_logger.log("Unable to write to KVS server", 40);
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// pass a fd and row to perform DELETEROW(r)
std::vector<char> FeUtils::kv_rename_row(int fd, std::vector<char> oldrow, std::vector<char> newrow)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "RNMR";
    std::vector<char> fn_string(cmd.begin(), cmd.end());

    fe_utils_logger.log("RNMR", LOGGER_DEBUG);
    fe_utils_logger.log("old row" + std::string(oldrow.begin(), oldrow.end()), LOGGER_DEBUG);
    fe_utils_logger.log("new row" + std::string(oldrow.begin(), oldrow.end()), LOGGER_DEBUG);

    insert_arg(fn_string, oldrow);
    insert_arg(fn_string, newrow);
    // fn_string.push_back('\b');
    std::vector<char> response = {};

    fe_utils_logger.log("Sending message:" + std::string(fn_string.begin(), fn_string.end()), LOGGER_DEBUG);

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        fe_utils_logger.log("Unable to write to KVS server", 40);
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

// pass a fd and row to perform DELETEROW(r)
std::vector<char> FeUtils::kv_rename_col(int fd, std::vector<char> row, std::vector<char> oldcol, std::vector<char> newcol)
{
    // string to send  COMMAND + 2<SP> + row + 2<SP> + col....
    std::string cmd = "RNMC";
    std::vector<char> fn_string(cmd.begin(), cmd.end());
    insert_arg(fn_string, row);
    insert_arg(fn_string, oldcol);
    insert_arg(fn_string, newcol);
    // fn_string.push_back('\b');
    std::vector<char> response = {};

    fe_utils_logger.log("Sending message:" + std::string(fn_string.begin(), fn_string.end()), LOGGER_DEBUG);

    // send message to kvs and check for error
    if (writeto_kvs(fn_string, fd) == 0)
    {
        // potentially logger
        fe_utils_logger.log("Unable to write to KVS server", 40);
        response = {'-', 'E', 'R'};
        return response;
    }

    // wait to recv response from kvs
    response = readfrom_kvs(fd);

    // return value
    return response;
}

/// @brief helper function to url encode a string
/// @param value string to url encode 
/// @return url encoded string
std::string FeUtils::urlEncode(const std::string value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        // keep alphanumeric and other accepted characters as is
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // remaining characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

/// @brief helper function to url dencode a string
/// @param value string to url dencode 
/// @return string that has been decoded
std::string FeUtils::urlDecode(const std::string value) {
    std::ostringstream decoded; // This will hold the decoded result

    //loop over each character in the string
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i]; //get the current character

        if (c == '+') { //'+' in URLs represents a space
            decoded << ' ';
        } else if (c == '%' && i + 2 < value.size()) {
            //if '%' is found and there are at least two characters after it
            std::string hexValue = value.substr(i + 1, 2); //extract the next two characters
            int charValue; //store the converted hexadecimal value
            std::istringstream(hexValue) >> std::hex >> charValue; //cnvert hex to int
            decoded << static_cast<char>(charValue); //cast the int to char and add to the stream
            i += 2; //skip the next two characters that were part of the hex value
        } else {
            decoded << c; //add the character as is if it's not a special case char
        }
    }

    return decoded.str(); //cnvert stream to string
}
