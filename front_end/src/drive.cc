/*
 * drive.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: aashok12
 */

#include "../include/drive.h"
#include "../../http_server/include/http_server.h"
#include "../../utils/include/utils.h"
#include <iostream>

// Folder handlers

// @todo: check path vs parameters of request

void open_folder(const HttpRequest &req, HttpResponse &res)
{
    // check req method
    if (req.req_method == "GET")
    {   
        // path is /api/drive/dirpath
        std::string dirpath = req.path.substr(11);
        std::cout << "GET request received for directory: " << dirpath << std::endl;
        const char* req_data = req.body.data(); // get raw string bytes
        const size_t req_data_size = req.body.size(); // get size of folder name
        std::cout << "User wants to open folder: " << std::string(req_data, req_data_size) << std::endl;
        
        std::string folderpath = dirpath + "/" + std::string(req_data, req_data_size);

        // // @note: assuming path is current dir path, same as row
        // // send get(dirpath,folderpath) to backend
        // // rowhash = get (....)
        // if (rowhash == ""){
        //     // value of get is the hash of the row that this lives on 
        //     error in folder
        //     call logger 
        //     return;
        // }

        // // using value, get value from rowhash, index -- @todo assuming all rows have index list?
        // // @note: maybe we can have a getcols(row) for backend which will get get col names for given row?
        // collist = get(rowhash, index);  //can we always guarantee, or need a check?

        // // parse collist for all headers
        // // @todo: some parse utility function ()
        
        std::vector<char> resp_body;


        
        // connect to backend
        // get(path,constructedpath)

        // Set appropriate response code and message
        res.code = 200;
        res.reason = "OK";
        res.body = resp_body;

        // Your logic to set response body, headers, etc. goes here
        // For simplicity, let's leave the response body empty
    }
    else
    {
        // If the request method is not GET, set an appropriate error response
        res.code = 405; // Method Not Allowed
        res.reason = "Method Not Allowed";
        const char* message = "Only GET method is allowed for file retrieval.";
        std::vector<char> resp_body(message, message + strlen(message));
        res.body = resp_body;
    }
}