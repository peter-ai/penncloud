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

#include "../include/drive.h"
#include "../../http_server/include/http_server.h"
#include "../../utils/include/utils.h"
#include "../utils/include/fe_utils.h"

// Folder handlers

// @todo: check path vs parameters of request

std::vector<char> content_vec = {'c', 'o', 'n', 't', 'e', 'n', 't'};

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

void open_filefolder(const HttpRequest &req, HttpResponse &res)
{
    // check req method

    // path is /api/drive/:childpath where parent dir is the page that is being displayed

    std::string childpath_str = req.path.substr(11);
    std::vector<char> child_path(childpath_str.begin(), childpath_str.end());

    std::cout << "GET request received for child path: " << childpath_str << std::endl;

    // int sockfd = FeUtils::open_socket();

    // if we are looking up a folder, use get row
    if (is_folder(child_path))
    {
        std::cout << "Looking up folder" << std::endl;

        std::string bodycont = "Folder: " + childpath_str;
        std::vector<char> body_vec(bodycont.begin(), bodycont.end());
        res.set_code(200);
        res.append_body_bytes(body_vec.data(), body_vec.size());

        // append header for content length

        // std::vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

        // if (kv_successful(folder_content))
        // {
        //     //@todo: update with html!
        //     res.append_body_bytes(folder_content.data(), folder_content.size());

        //     // append header for content length
        //     std::string content_header = "Content-Length:";
        //     std::string header_value = std::to_string(folder_content.size());
        //     res.set_header(content_header, header_value);
        //     res.set_code(200);
        // }
        // else
        // {
        //     res.set_code(400);
        // }
    }
    else
    {
        // file, need to get parent row and file name
        std::string filename;
        std::string parentpath_str = split_parent_filename(Utils::split(childpath_str, "/"), filename);
        std::cout << "Parent path is: " << parentpath_str.c_str() << std::endl;
        std::cout << "File is: " << filename.c_str() << std::endl;

        std::vector<char> parent_path_vec(parentpath_str.begin(), parentpath_str.end());
        std::vector<char> filename_vec(filename.begin(), filename.end());

        std::cout << "Looking up file" << std::endl;

        std::string bodycont = "File: " + filename;
        std::vector<char> body_vec(bodycont.begin(), bodycont.end());
        res.set_code(200);
        res.append_body_bytes(body_vec.data(), body_vec.size());

        // // get file content
        // std::vector<char> file_content = FeUtils::kv_get(sockfd, parent_path_vec, filename_vec);

        // if (kv_successful(file_content))
        // {
        //     res.append_body_bytes(file_content.data(), file_content.size());

        //     res.set_header(content_header, header_value);
        //     res.set_code(200);
        // }
        // else
        // {
        //     res.set_code(400);
        // }
    }

    // close(sockfd);
}

void upload_file(const HttpRequest &req, HttpResponse &res)
{

    // Check if the request contains a body
    if (!req.body_as_bytes().empty())
    {

        // Copy request body to response body
        std::vector<char> body_content = req.body_as_bytes();
        res.append_body_bytes(body_content.data(), body_content.size());

        std::cout << "Content-Disposition header" << std::endl;
        std::vector<std::string> headers = req.get_header("Content-Disposition");
        for (const auto &header : headers)
        {
            std::cout << header << std::endl;
        }


         std::cout << "Content-Type header" << std::endl;
        headers = req.get_header("Content-Type");
        for (const auto &header : headers)
        {
            std::cout << header << std::endl;
        }

          // int sockfd = FeUtils::open_socket();
           // std::vector<char> file_content = FeUtils::kv_get(sockfd, par, filename_vec);

        

        // Respond with a success message
        std::string response_body = "File uploaded successfully";

        // @todo should we instead get row for the page they are on?
        res.set_code(200); // OK


    }
    else
    {
        // No body found in the request
        res.set_code(400); // Bad Request
    }
}