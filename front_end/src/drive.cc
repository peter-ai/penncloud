/*
 * drive.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: aashok12
 */

#include "../include/drive.h"
#include "../../http_server/include/http_server.h"


// Folder handlers
void handleGetFile(const HttpRequest& req, HttpResponse& res);