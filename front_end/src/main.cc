/*
 * main.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: cis5050
 */

#include <iostream>
#include "../../http_server/include/http_server.h"
#include "../utils/include/fe_utils.h"
#include "../include/mailbox.h"
#include "../include/authentication.h"
#include "../include/authentication.h"



using namespace std;

atomic<bool> runServer(true);

void contactLoadBalancer()
{
}

int main()
{
	// add routes to routing table
	// get mailbox
	HttpServer::get("/api/:user/mbox", mailbox_handler);
	// send an email
	HttpServer::post("/api/:user/mbox/send", sendEMail_handler);
	// get email
	HttpServer::get("/api/:user/mbox?", email_handler);
	// respond to an email
	HttpServer::post("/api/:user/mbox/reply?", replyEmail_handler);
	// forward an email
	HttpServer::post("/api/:user/mbox/forward?", forwardEmail_handler);
	// delete an email
	HttpServer::post("/api/:user/mbox/delete?", deleteEmail_handler);

	// run HTTPServer
	HttpServer::run(8000);

	// perform front end specific tasks
	while (runServer)
	{
		// Example: Dispatch a thread that contacts the load balancer here
		thread lbThread(contactLoadBalancer);
		lbThread.join(); // Wait for the server thread to finish
	}

	return 0;
}
