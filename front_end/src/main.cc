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
#include "../include/drive.h"


using namespace std;

atomic<bool> runServer(true);

void contactLoadBalancer()
{
}

int main()
{
	/* Mail Routes */

	HttpServer::post("/api/:user/mbox/send", sendEMail_handler);// send an email
	HttpServer::post("/api/:user/mbox/reply?", replyEmail_handler);// respond to an email
	HttpServer::post("/api/:user/mbox/forward?", forwardEmail_handler);// forward an email
	HttpServer::post("/api/:user/mbox/delete?", deleteEmail_handler);// delete an email
	HttpServer::get("/api/:user/mbox", mailbox_handler); // get mailbox
	HttpServer::get("/api/:user/mbox?", email_handler);// get email

	/* Auth Routes */
	HttpServer::post("/api/signup", signup_handler); // signup 
	HttpServer::post("/api/login", login_handler); // login
	HttpServer::post("/api/logout", logout_handler); // logout
	HttpServer::post("/api/update_password", update_password_handler); // update password

	/* Drive Routes */	
	HttpServer::post("/api/drive/upload/*", upload_file); // upload file
	HttpServer::get("/api/drive/*", open_filefolder); // open file/folder

	// run HTTPServer
	HttpServer::run(8000);

	// // perform front end specific tasks
	// while (runServer)
	// {
	// 	// Example: Dispatch a thread that contacts the load balancer here
	// 	thread lbThread(contactLoadBalancer);
	// 	lbThread.join(); // Wait for the server thread to finish
	// }

	return 0;
}
