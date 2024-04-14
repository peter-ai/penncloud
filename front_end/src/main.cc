/*
 * main.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: cis5050
 */

#include <iostream>
#include "http_server.h"
#include "fe_utils.h"
#include <thread>
#include <map>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <vector>
#include <utility>

using namespace std;

atomic<bool> runServer(true);

volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int sig) {
	if (sig == SIGINT) {
		shutdown_requested = 1;
		exit(0);
	}
}
/**
 * Receive backend server's response
 */


string parseMailboxPathToRowKey(string path) {
	string rowKey = "";
	size_t startPos = path.find('/', 1); // Start after the first character
	if (startPos != std::string::npos) {
		// Extract from the second slash to the end of the string
		rowKey = path.substr(startPos + 1);
	}
	return rowKey;
}

bool startsWith(const std::vector<char> &vec, const std::string &prefix) {
	if (vec.size() < prefix.size())
		return false;
	return std::string(vec.begin(), vec.begin() + prefix.size()) == prefix;
}


// /api/user/mailbox/send?uidl=12345
std::string get_query_parameter(const HttpRequest &request, const std::string& key) {
        std::unordered_map<std::string, std::string> query_params;
        size_t queryStart = request.path.find('?');
        if (queryStart != std::string::npos) {
            std::string queryString = request.path.substr(queryStart + 1);
            std::istringstream queryStream(queryString);
            std::string param;
            while (std::getline(queryStream, param, '&')) {
                size_t equals = param.find('=');
                if (equals != std::string::npos) {
                    std::string param_key = param.substr(0, equals);
                    std::string param_value = param.substr(equals + 1);
                    query_params[param_key] = param_value;
                }
            }
        }
        // Attempt to find the key in the parsed query parameters
        auto it = query_params.find(key);
        if (it != query_params.end()) {
            return it->second;
        }
        return "";  // Return empty string if key is not found
    }
//HANDLERS//

//UIDL: time, to, from, subject

//email format

//time: Fri Mar 15 18:47:23 2024
//"to": "recipient@example.com",
//"from": "sender@example.com",
//"subject": "Your Subject Here",
//"body": "Hello, this is the body of the email."

void forwardEmail_handler(const HttpRequest &request, HttpResponse &response) {
    if (request.method == "POST") {
        int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
        if (socket_fd < 0) {
            response.set_code(501);
            response.append_body_str("Failed to open socket.");
            return;
        }

        // Extract the email ID and destination address from the query or body
        string rowKey = parseMailboxPathToRowKey(request.path);
        string colKey = get_query_parameter(request, "uidl");
        vector<char>row(rowKey.begin(), rowKey.end());
        vector<char>col(colKey.begin(), colKey.end());

        // Fetch the email from KVS
        vector<char> emailData = FeUtils::kv_get(socket_fd, row, col);

        if (emailData.empty()) {
            response.set_code(404);
            response.append_body_str("Email not found.");
            return;
        }

        string forwardTo = get_query_parameter(request, "forwardTo") + "-mailbox/";  // Assume this is passed
        vector<char>forward(forwardTo.begin(), forwardTo.end());

        // Forward the email - simplistic simulation of forwarding
        vector<char> kvsResponse = FeUtils::kv_put(socket_fd, forward, col, emailData);

		if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK")) {
			response.set_code(200); // Success
			response.append_body_str("+OK Email forwarded successfully.");
		} else {
			response.set_code(501); // Internal Server Error
			response.append_body_str("-ER Failed to forward email.");
		}

		response.set_header("Content-Type", "text/html");
		response.set_header("Content-Length", response.body_size());
		close(socket_fd);
    }
}

void replyEmail_handler(const HttpRequest &request, HttpResponse &response) {
}

void deleteEmail_handler(const HttpRequest &request, HttpResponse &response) {
    if (request.method == "DELETE") {
        int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
        if (socket_fd < 0) {
            response.set_code(501);
            response.append_body_str("Failed to open socket.");
            return;
        }
        string rowKey = parseMailboxPathToRowKey(request.path);
        vector<char>row(rowKey.begin(), rowKey.end());

        string emailId = get_query_parameter(request, "uidl");  // UID of the email to delete
        vector<char> col(emailId.begin(), emailId.end());
        vector<char> kvsResponse = FeUtils::kv_del(socket_fd, row, col);


		if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK")) {
			response.set_code(200); // Success
			response.append_body_str("+OK Email deleted successfully.");
		} else {
			response.set_code(501); // Internal Server Error
			response.append_body_str("-ER Failed to delete email.");
		}
        close(socket_fd);
    }
}

void email_handler(const HttpRequest &request, HttpResponse &response) {
	if (request.method == "POST") {
		int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
		if (*socket_fd < 0) {
			response.set_code(501);
			response.append_body_str("Failed to open socket.");
			return;
		}
	}
}

void sendEMail_handler(const HttpRequest &request, HttpResponse &response) {
	if (request.method == "POST") {
		int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
		if (*socket_fd < 0) {
			response.set_code(501);
			response.append_body_str("Failed to open socket.");
			return;
		}
		string rowKey = parseMailboxPathToRowKey(request.path);

		//this will be the UIDL of the email
		string colKey = get_query_parameter(request, "uidl");

		vector<char> value = move(request.body_as_bytes());
		vector<char> row(rowKey.begin(), rowKey.end());
		vector<char> col(colKey.begin(), colKey.end());

		vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);

		if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK")) {
			response.set_code(200); // Success
			response.append_body_str("+OK Email sent successfully.");
		} else {
			response.set_code(501); // Internal Server Error
			response.append_body_str("-ER Failed to send email.");
		}

		response.set_header("Content-Type", "text/html");
		response.set_header("Content-Length", response.body_size());
		close(socket_fd);
	}
}

void mailbox_handler(const HttpRequest &request, HttpResponse &response) {
	if (request.method == "GET") {
		int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
		if (*socket_fd < 0) {
			response.set_code(501);
			response.append_body_str("Failed to open socket.");
			return;
		}
		//path is: /api/mailbox/{username}/
		string rowKey = parseMailboxPathToRowKey(request.path);
		vector<char> row(rowKey.begin(), rowKey.end());
		vector<char> kvsValue = FeUtils::kv_get_row(socket_fd, row);

		if (startsWith(kvsValue, "+OK")) {
			response.set_code(200);  // OK
			char *charPointer = kvsValue.data();
			response.append_body_bytes(kvsValue.data() + 3,
					kvsValue.size() - 3);
		} else {
			response.append_body_str("Error processing request.");
			response.set_code(400);  // Bad request
		}
		response.set_header("Content-Type", "text/html");
		response.set_header("Content-Length", response.body_size());
		close(socket_fd);
	}
	//will need to sort according to which page is shown
	//end of handler --> http server sends response back to client
}

void contactLoadBalancer(){

}

int main() {
	//set up signal handler
	signal(SIGINT, signal_handler);
	//run HTTPServer
	HttpServer server = HttpServer();

	//add routes to routing table
	server.get("/api/{username}/mbox", mailbox_handler); //retrieves malbos
	server.get("/api/{username}/mbox/{emailId}", email_handler); //retrieving a specific email

	server.post("/api/user/mailbox/send", sendEMail_handler);
	server.post("/api/user/mailbox/reply", replyEmail_handler);
	server.post("/api/user/mailbox/forward", forwardEmail_handler);

	server.delete("/api/user/mailbox/delete", deleteEmail_handler);


	// Start the HTTP server in a separate thread
	thread serverThread([&server]() {
		server.run(8080);  // Run on port 8080
	});

//perform front end specific tasks
	while (runServer) {
		// Example: Dispatch a thread that contacts the load balancer here
		thread lbThread(contactLoadBalancer);

		lbThread.join();
		// std::thread lbThread(contactLoadBalancer);

		//thread level socket that services 1 client

		//each socket
		//send fd command to utility function that takes a socket

		lbThread.join(); // Wait for the server thread to finish
	}

	return 0;
}
