/*
 * main.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: cis5050
 */
#include "../../http_server/include/http_server.h"
#include "../include/authentication.h"
#include "../include/response_codes.h"
#include "../include/mailbox.h"
#include "../include/drive.h"

using namespace std;

int main(int argc, char* argv[])
{
	int opt;
    int port;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg); // Convert the port number from string to int
                break;
            default: // '?' is returned by getopt for unrecognized option
                std::cerr << "Usage: " << argv[0] << " -p <port>\n";
                return 1;
        }
    }

	/* Public GET API */
	HttpServer::get("/home", home_page);
	HttpServer::get("/account", update_password_page);
	HttpServer::get("/update_success", update_password_success_page);
	HttpServer::get("/409", error_409_page); // Conflict
	HttpServer::get("/401", error_401_page); // Unauthorized
	HttpServer::get("/400", error_400_page); // Bad API request
	

	/* Mail Routes */
	HttpServer::post("/api/:user/mbox/send", sendEmail_handler);		// send an email
	HttpServer::post("/api/:user/mbox/reply?", replyEmail_handler);		// respond to an email
	HttpServer::post("/api/:user/mbox/forward?", forwardEmail_handler); // forward an email
	HttpServer::post("/api/:user/mbox/delete?", deleteEmail_handler);	// delete an email
	HttpServer::get("/:user/mbox", mailbox_handler);					// get mailbox
	HttpServer::get("/:user/mbox?", email_handler);						// get email

	/* Auth Routes */
	HttpServer::post("/api/signup", signup_handler);				   // signup
	HttpServer::post("/api/login", login_handler);					   // login
	HttpServer::post("/api/logout", logout_handler);				   // logout
	HttpServer::post("/api/update_password", update_password_handler); // update password

	/* Drive Routes */
	HttpServer::post("/api/drive/upload/*", upload_file); // upload file
	HttpServer::post("/api/drive/delete/*", delete_filefolder); // delete a file
	HttpServer::post("/api/drive/create/*", create_folder); // create a folder
	HttpServer::get("/drive/*", open_filefolder);		  // open file/folder

	// run HTTPServer
	HttpServer::run(port);

	return 0;
}
