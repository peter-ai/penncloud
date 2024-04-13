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

std::vector<char> content_vec = {'c','o','n','t','e','n','t'};

// helper to return parent path
std::string constr_parentpath(const std::vector<std::string>& vec) {
    std::string parentpath;

    // Iterate over all elements except the last one
    for (size_t i = 0; i < vec.size() - 1; ++i) {
        parentpath += vec[i]; // Append the current element to the result string
    }

    return parentpath;
}


bool kv_successful(const std::vector<char>& vec) {
    // Check if the vector has at least 3 characters
    if (vec.size() < 3) {
        return false;
    }

    // Define the expected prefix
    std::vector<char> prefix = {'+', 'O', 'K'};

    // Check if the first three characters match the prefix
    return std::equal(prefix.begin(), prefix.end(), vec.begin());
}

void open_filefolder(const HttpRequest &req, HttpResponse &res, std::string ipaddr, int port)
{
    // check req method
    if (req.method == "GET")
    {
        // path is /api/drive/:childpath where parent dir is the page that is being displayed

        std::string childpath_str = req.path.substr(11);
        std::vector<char> child_path (childpath_str.begin(), childpath_str.end());


        std::cout << "GET request received for child path: " << childpath_str << std::endl;

        //construct parent path string
        std::string parentpath_str = constr_parentpath(Utils::split(childpath_str,"/"));


        std::cout << "Parent path is: " << parentpath_str.c_str() << std::endl;

        // Construct parent path vector
        std::vector<char> parent_path(parentpath_str.begin(), parentpath_str.end());
         
        int sockfd = FeUtils::open_socket();
        
        // currently assuming options are 'f' for file or 'd' for directory
        std::vector<char> ftype = FeUtils::kv_get(sockfd, parent_path, child_path);

        if (kv_successful){

        // @todo change if any meta data is added
        if (ftype[4] == 'f'){
            // file 
            // follow format for file with row = path, col = 'content', val = data
            std::vector<char> file_content = FeUtils::kv_get(sockfd, child_path, content_vec);
            res.append_body_bytes(file_content.data(), file_content.size());

            // append header for content length
            std::string content_header = "Content-Length:";
            std::string header_value = std::to_string(file_content.size());
            res.set_header(content_header,header_value);
            res.set_code(200);

        } else if (ftype[4] == 'd'){
            // this is a folder or directory
            std::vector<char> folder_content = FeUtils::kv_get_row(sockfd, child_path);

            //@todo: update with html!
            res.append_body_bytes(folder_content.data(), folder_content.size());

            // append header for content length
            std::string content_header = "Content-Length:";
            std::string header_value = std::to_string(folder_content.size());
            res.set_header(content_header,header_value);
            res.set_code(200);

        }

        } else {
           res.set_code(400);
        }
    }
    else
    {
        // If the request method is not GET, set an appropriate error response
        res.set_code(405); // Method Not Allowed
        std::cerr << "Only GET method is allowed for file retrieval." << std::endl;
    }
}