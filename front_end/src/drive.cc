/*
 * drive.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: aashok12
 */
#include "../include/drive.h"
using namespace std;
// Folder handlers

vector<char> ok_vec = {'+', 'O', 'K', ' '};
vector<char> err_vec = {'-', 'E', 'R', ' '};

// helper to return parent path
string split_parent_filename(const vector<string> &vec, string &filename)
{
    string parentpath;

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        parentpath += vec[i]; // Append the current element to the result string
        parentpath += '/';
    }

    filename = vec.back();

    return parentpath;
}

// gets username from file path
string get_username(const string path)
{
    return Utils::split_on_first_delim(path, "/")[0];
}

// checks if path ends in /, if yes folder. Otherwise of type file
bool is_folder(const vector<char> &vec)
{
    return vec.back() == '/';
}

bool kv_successful(const vector<char> &vec)
{
    // Check if the vector has at least 3 characters
    if (vec.size() < 3)
    {
        return false;
    }

    // Define the expected prefix
    vector<char> prefix = {'+', 'O', 'K'};

    // Check if the first three characters match the prefix
    return equal(prefix.begin(), prefix.end(), vec.begin());
}

// Function to split a vector<char> based on a vector<char> delimiter
vector<vector<char>> split_vector(const vector<char> &data, const vector<char> &delimiter)
{
    vector<vector<char>> result;
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

// Function to split a vector<char> based on the first occurrence of a vector<char> delimiter
vector<vector<char>> split_vec_first_delim(const vector<char> &data, const vector<char> &delimiter)
{
    vector<vector<char>> result;
    size_t start = 0;

    // Find the first occurrence of delimiter in data
    auto pos = search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

    if (pos == data.end())
    {
        // No delimiter found, return the whole vector as a single part
        result.emplace_back(data.begin(), data.end());
    }
    else
    {
        // Delimiter found, split at the delimiter
        result.emplace_back(data.begin() + start, pos);          // Part before the delimiter
        result.emplace_back(pos + delimiter.size(), data.end()); // Part after the delimiter
    }

    return result;
}

// helper to return parent path
vector<char> format_folder_contents(vector<vector<char>> &vec)
{
    vector<char> output;

    if (vec.size() == 0)
    {
        return output;
    }

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        output.insert(output.end(), (vec[i]).begin(), (vec[i]).end());
        output.push_back(',');
        output.push_back(' ');
    }

    output.insert(output.end(), (vec.back()).begin(), (vec.back()).end());

    return output;
}

// helper to check if a vecotr of chars contains a subseqeunce
bool contains_subseq(const vector<char> &sequence, const vector<char> &subsequence)
{
    // convert both vectors to strings
    string seq_str(sequence.begin(), sequence.end());
    string subseq_str(subsequence.begin(), subsequence.end());

    // using search to find the subsequence in the sequence
    return search(seq_str.begin(), seq_str.end(), subseq_str.begin(), subseq_str.end()) != seq_str.end();
}

void open_filefolder(const HttpRequest &req, HttpResponse &res)
{
    // check req method

    // path is drive/:childpath where parent dir is the page that is being displayed
    string childpath_str = req.path.substr(6);
    string username = get_username(childpath_str);
    vector<char> child_path(childpath_str.begin(), childpath_str.end());

    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    // if we are looking up a folder, use get row
    if (is_folder(child_path))
    {

        vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

        if (kv_successful(folder_content))
        {
            // content list, remove '+OK<sp>'
            vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
            // split on delim
            vector<vector<char>> contents = split_vector(folder_elements, {'\b'});
            vector<char> formatted_content = format_folder_contents(contents);

            //@todo: update with html!
            res.append_body_bytes(formatted_content.data(), formatted_content.size());

            // append header for content length
            res.set_code(200);
        }
        else
        {
            res.set_code(400);
        }
    }
    else
    {
        // file, need to get parent row and file name
        string filename;
        string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);

        vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> filename_vec(filename.begin(), filename.end());

        // get file content
        vector<char> file_content = FeUtils::kv_get(sockfd, parent_path_vec, filename_vec);

        if (kv_successful(file_content))
        {
            // get binary from 4th char onward (ignore +OK<sp>)
            vector<char> file_binary(file_content.begin() + 4, file_content.end());

            // apend to body
            res.append_body_bytes(file_binary.data(), file_binary.size());

            // set code
            res.set_code(200);
        }
        else
        {
            // @todo ask about error codes
            res.set_code(400);
        }
    }

    // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}

void upload_file(const HttpRequest &req, HttpResponse &res)
{

    // Get path of parent directory where we are appending

    // path is /api/drive/upload/:parentpath where parent dir is the page that is being displayed
    string parentpath_str = req.path.substr(18);
    string username = get_username(parentpath_str);

    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    // Check if the request contains a body
    if (!req.body_as_bytes().empty())
    {

        // ------- Get file name and clean up to get file binary ----
        // Get the single file uploaded
        vector<string> headers;

        // Find form boundary
        headers = req.get_header("Content-Type");
        string header_str(headers[0]);
        // boundary provided by form
        vector<string> find_boundary = Utils::split_on_first_delim(header_str, "boundary=");
        vector<char> bound_vec(find_boundary.back().begin(), find_boundary.back().end());

        vector<char> line_feed = {'\r', '\n'};

        // split body on boundary
        vector<vector<char>> elements = split_vector(req.body_as_bytes(), bound_vec);
        vector<char> elem1 = elements[1];

        // @note: assuming we only upload 1 file at a time?
        vector<char> file_data = split_vec_first_delim(elem1, line_feed)[1];

        // split file data to separate file metadata from binary values
        vector<vector<char>> body_elems = split_vec_first_delim(file_data, line_feed);

        // parse file headers to get name of file
        string file_headers(body_elems[0].begin(), body_elems[0].end());
        string content_disp = Utils::split_on_first_delim(file_headers, "\n")[0];
        string filename_toparse = Utils::split_on_first_delim(file_headers, "filename=").back();

        // file name string
        string filename = Utils::split(filename_toparse, "\"")[0];

        // get file binary
        vector<char> file_binary = split_vec_first_delim(body_elems[1], line_feed)[1];
        file_binary = split_vec_first_delim(file_binary, line_feed)[1];

        if (parentpath_str.back() != '/')
        {
            res.set_code(400);
              // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);
            close(sockfd);
            return;
        }
        string childpath_str = parentpath_str + filename;

        vector<char> row_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> col_vec(filename.begin(), filename.end());

        vector<char> kvs_resp = FeUtils::kv_put(sockfd, row_vec, col_vec, file_binary);

        if (kv_successful(kvs_resp))
        {
            // @todo should we instead get row for the page they are on?
            res.set_code(200); // OK
            vector<char> folder_content = FeUtils::kv_get_row(sockfd, row_vec);

            // content list, remove '+OK<sp>'
            vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
            // split on delim
            vector<vector<char>> contents = split_vector(folder_elements, {'\b'});
            vector<char> formatted_content = format_folder_contents(contents);

            //@todo: update with html!
            res.append_body_bytes(formatted_content.data(), formatted_content.size());
        }
        else
        {
            res.set_code(400);
            // maybe retry? tbd
        }
    }
    else
    {
        // No body found in the request
        res.set_code(400); // Bad Request
    }

    // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}

// creates a new folder
void create_folder(const HttpRequest &req, HttpResponse &res)
{
    // uses a post request to add a new folder to the current parent directory.

    // path is /api/drive/create/:parentpath where parent dir is the page that is being displayed
    string parentpath_str = req.path.substr(18);

    string username = get_username(parentpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    string req_body = req.body_as_string();

    // check that ody is not empty
    if (!req_body.empty())
    {
        // get name of folder
        string key = "name=";
        vector<string> elements = Utils::split_on_first_delim(req_body, key);

        // if key doesn't exist, return 400
        if (elements.size() < 1)
        {
            res.set_code(400);
            return;
        }

        vector<char> folder_name(elements[0].begin(), elements[0].end());
        folder_name.push_back('/');
        vector<char> row_name(parentpath_str.begin(), parentpath_str.end());

        vector<char> folder_content = FeUtils::kv_get_row(sockfd, row_name);

        // content list, remove '+OK<sp>'
        vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
        // split on delim
        vector<vector<char>> contents = split_vector(folder_elements, {'\b'});
        vector<char> formatted_content = format_folder_contents(contents);

        // if folder name in use
        if (contains_subseq(formatted_content, folder_name))
        {
            // currently returning 400 but not sure what behavior should be
            res.set_code(400);
        }
        else
        {
            if (kv_successful(FeUtils::kv_put(sockfd, row_name, folder_name, {})))
            {

                // create new column for row
                vector<char> folder_row = row_name;
                folder_row.insert(folder_row.end(), folder_name.begin(), folder_name.end());
                vector<char> kvs_resp = FeUtils::kv_put(sockfd, folder_row, {}, {});

                // get parent folder to show that this folder has been nested
                folder_content = FeUtils::kv_get_row(sockfd, row_name);

                // content list, remove '+OK<sp>'
                vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
                contents = split_vector(folder_elements, {'\b'});
                formatted_content = format_folder_contents(contents);
                res.append_body_bytes(formatted_content.data(), formatted_content.size());
                res.set_code(200);
            }
            else
            {
                // logger error
                res.set_code(400);
            }
        }
    }
    else
    {
        res.set_code(400);
    }

      // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}

// deletes file or folder
// @todo is this a post or a get? I think post with no body?
void delete_filefolder(const HttpRequest &req, HttpResponse &res)
{

    // of type /api/drive/delete/* where child directory is being served
    string childpath_str = req.path.substr(18);
    string username = get_username(childpath_str);
    bool present = HttpServer::check_kvs_addr(username);
    std::vector<std::string> kvs_addr;

    // check if we know already know the KVS server address for user
    if (present)
    {
        kvs_addr = HttpServer::get_kvs_addr(username);
    }
    // otherwise get KVS server address from coordinator
    else
    {
        // query the coordinator for the KVS server address
        kvs_addr = FeUtils::query_coordinator(username);
    }

    // create socket for communication with KVS server
    int sockfd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

    // validate session id
    string valid_session_id = FeUtils::validate_session_id(sockfd, username, req);
    // if invalid, return an error?
    // @todo :: redirect to login page?
    if (valid_session_id.empty())
    {
        // for now, returning code for check on postman
        res.set_code(401);
        close(sockfd);
        return;
    }

    vector<char> child_path(childpath_str.begin(), childpath_str.end());

    // if we are trying to delete a file
    if (!is_folder(child_path))
    {
        // get file name
        string filename;
        string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);

        // comver tto vector<char>
        vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        vector<char> filename_vec(filename.begin(), filename.end());


        if (kv_successful(FeUtils::kv_del(sockfd, parent_path_vec, filename_vec)))
        {
            // success
            res.set_code(200);

            // reload page to show file has been deleted
            vector<char> folder_content = FeUtils::kv_get_row(sockfd, parent_path_vec);

            // content list, remove '+OK<sp>'
            vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
            vector<vector<char>> contents = split_vector(folder_elements, {'\b'});
            vector<char> formatted_content = format_folder_contents(contents);
            res.append_body_bytes(formatted_content.data(), formatted_content.size());
            res.set_code(200);
        }
        else
        {
            res.set_code(400);
        }

    }

      // set cookies on response
    FeUtils::set_cookies(res, username, valid_session_id);

    close(sockfd);
}