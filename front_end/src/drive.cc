/*
 * drive.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: aashok12
 */

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

#include "../include/drive.h"
#include "../../http_server/include/http_server.h"
#include "../../utils/include/utils.h"
#include "../utils/include/fe_utils.h"

// Folder handlers

std::vector<char> ok_vec = {'+', 'O', 'K', ' '};
std::vector<char> err_vec = {'-', 'E', 'R', ' '};

// helper to return parent path
std::string split_parent_filename(const std::vector<std::string> &vec, std::string &filename)
{
    std::string parentpath;

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i)
    {
        parentpath += vec[i]; // Append the current element to the result string
        parentpath += '/';
    }

    filename = vec.back();

    return parentpath;
}

// checks if path ends in /, if yes folder. Otherwise of type file
bool is_folder(const std::vector<char> &vec)
{
    return vec.back() == '/';
}

bool kv_successful(const std::vector<char> &vec)
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

// Function to split a vector<char> based on a vector<char> delimiter
std::vector<std::vector<char>> split_vector(const std::vector<char> &data, const std::vector<char> &delimiter)
{
    std::vector<std::vector<char>> result;
    size_t start = 0;
    size_t end = data.size();

    while (start < end)
    {
        // Find the next occurrence of delimiter starting from 'start'
        auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

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
            start = std::distance(data.begin(), pos) + delimiter.size();
        }
    }

    return result;
}

// Function to split a vector<char> based on the first occurrence of a vector<char> delimiter
std::vector<std::vector<char>> split_vec_first_delim(const std::vector<char> &data, const std::vector<char> &delimiter)
{
    std::vector<std::vector<char>> result;
    size_t start = 0;

    // Find the first occurrence of delimiter in data
    auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

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
std::vector<char> format_folder_contents(std::vector<std::vector<char>> &vec)
{
    std::vector<char> output;

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


void open_filefolder(const HttpRequest &req, HttpResponse &res)
{
    // check req method

    // path is /api/drive/:childpath where parent dir is the page that is being displayed

    std::string childpath_str = req.path.substr(11);
    std::vector<char> child_path(childpath_str.begin(), childpath_str.end());

    std::cout << "GET request received for child path: " << childpath_str << std::endl;

    int sockfd = FeUtils::open_socket();

    // if we are looking up a folder, use get row
    if (is_folder(child_path))
    {
        std::cout << "Looking up folder" << std::endl;

        std::string bodycont = "Folder: " + childpath_str;

        std::vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

        if (kv_successful(folder_content))
        {
            // content list, remove '+OK<sp>'
            std::vector<char> folder_elements(folder_content.begin() + 4, folder_content.end());
            // split on delim
            std::vector<std::vector<char>> contents = split_vector(folder_elements, {'\b'});
            std::vector<char> formatted_content = format_folder_contents(contents);

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
        std::string filename;
        std::string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);
    
        std::vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        std::vector<char> filename_vec(filename.begin(), filename.end());

        // get file content
        std::vector<char> file_content = FeUtils::kv_get(sockfd, parent_path_vec, filename_vec);

        if (kv_successful(file_content))
        {
            // get binary from 4th char onward (ignore +OK<sp>)
            std::vector<char> file_binary(file_content.begin() + 4, file_content.end());

            // apend to body
            res.append_body_bytes(file_binary.data(), file_binary.size());

            // // octet-steam for content header @todo -- setting this type means postman can't see it
            // std::string content_header = "Content-Type";
            // std::string content_value = "application/octet-stream";
            // res.set_header(content_header, content_value);

            // set code
            res.set_code(200);
        }
        else
        {
            // @todo ask about error codes
            res.set_code(400);
        }
    }

    close(sockfd);
}

void upload_file(const HttpRequest &req, HttpResponse &res)
{

    // Check if the request contains a body
    if (!req.body_as_bytes().empty())
    {

        // ------- Get file name and clean up to get file binary ----
        // Get the single file uploaded
        std::vector<std::string> headers;

        // Find form boundary
        headers = req.get_header("Content-Type");
        std::string header_str(headers[0]);
        // boundary provided by form
        std::vector<std::string> find_boundary = Utils::split_on_first_delim(header_str, "boundary=");
        std::vector<char> bound_vec(find_boundary.back().begin(), find_boundary.back().end());

        std::vector<char> line_feed = {'\r', '\n'};

        // split body on boundary
        std::vector<std::vector<char>> elements = split_vector(req.body_as_bytes(), bound_vec);
        std::vector<char> elem1 = elements[1];

        // @note: assuming we only upload 1 file at a time?
        std::vector<char> file_data = split_vec_first_delim(elem1, line_feed)[1];

        // split file data to separate file metadata from binary values
        std::vector<std::vector<char>> body_elems = split_vec_first_delim(file_data, line_feed);

        // parse file headers to get name of file
        std::string file_headers(body_elems[0].begin(), body_elems[0].end());
        std::string content_disp = Utils::split_on_first_delim(file_headers, "\n")[0];
        std::string filename_toparse = Utils::split_on_first_delim(file_headers, "filename=").back();

        // file name string
        std::string filename = Utils::split(filename_toparse, "\"")[0];

        // get file binary
        std::vector<char> file_binary = split_vec_first_delim(body_elems[1], line_feed)[1];
        file_binary = split_vec_first_delim(file_binary, line_feed)[1];

        // Get path of parent directory where we are appending

        // path is /api/drive/upload/:parentpath where parent dir is the page that is being displayed
        std::string parentpath_str = req.path.substr(18);
        std::string childpath_str = parentpath_str + filename;

        std::vector<char> row_vec(parentpath_str.begin(), parentpath_str.end());
        std::vector<char> col_vec(filename.begin(), filename.end());

        std::cout << "row: " <<parentpath_str << std::endl;
        std::cout << "column: " <<filename << std::endl;

        int sockfd = FeUtils::open_socket();

        std::vector<char> kvs_resp = FeUtils::kv_put(sockfd, row_vec, col_vec, file_binary);

        if (kv_successful(kvs_resp))
        {
            // @todo should we instead get row for the page they are on?
            res.set_code(200); // OK
            // res.append_body_bytes(file_binary.data(), file_binary.size());
        } else {
            res.set_code(400);
            // maybe retry? tbd

        }

        // @todo should we instead get row for the page they are on?
        res.set_code(200); // OK
        res.append_body_bytes(file_binary.data(), file_binary.size());
    }
    else
    {
        // No body found in the request
        res.set_code(400); // Bad Request
    }
}