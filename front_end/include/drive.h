/*
 * main.h
 *
 *  Created on: Apr 10, 2024
 *      Author: cis5050
 */

#ifndef FRONT_END_INCLUDE_DRIVE_H_
#define FRONT_END_INCLUDE_DRIVE_H_

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <vector>

#include "../../http_server/include/http_server.h"
#include "../../utils/include/utils.h"
#include "../utils/include/fe_utils.h"

void open_filefolder(const HttpRequest& req, HttpResponse& res);

void upload_file(const HttpRequest& req, HttpResponse& res);

void create_folder(const HttpRequest& req, HttpResponse& res);





#endif /* FRONT_END_INCLUDE_DRIVE_H_ */
