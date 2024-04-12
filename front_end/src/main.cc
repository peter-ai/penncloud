/*
 * main.cc
 *
 *  Created on: Apr 10, 2024
 *      Author: cis5050
 */

#include <iostream>
#include "http_server.h"
#include <thread>
#include <map>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

using namespace std;

map<string, string> clientToBackendMap;

//GET handler
//POST handler

/**
 * Receive backend server's response
 */

//frontend is stateless
char* processServerResponse(const char* backEndServerResponse, HttpResponse& response){

	char* processedResponse = nullptr;

	//appended with -ERR or +OK [fixed number of bytes]
	//GET(R,C) -> returns a value (vector<char>)
	//PUT(R, C, V)) --> returns a status code

	response.append_to_body(processedResponse);


	return processedResponse;
}

/**
 * Fetch mailbox data from backend
 */
string getMailboxContent(const string& userId) {
    string backendUrl = "https://backend-service-url/api/mailbox";
    backendUrl += "?userId=" + userId; // Assuming user ID is passed as a query parameter.

    // Prepare your HTTP request (GET method in this case).
    HttpRequest request(backendUrl, HttpMethod::GET);
    // Add any necessary headers here. For example, Authorization headers.
    request.addHeader("Authorization", "Bearer <your_access_token_here>");

    try {
        // Send the request to the backend server and wait for the response.
        HttpResponse response = HttpClient::sendRequest(request);

        if (response.statusCode == 200) {
            // Process the response data if the request was successful.
            return processServerResponse(response.body);
        } else {
            // Handle errors or unexpected status codes.
            std::cerr << "Failed to retrieve mailbox content, status code: " << response.statusCode << std::endl;
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred while trying to fetch mailbox content: " << e.what() << std::endl;
        return "";
    }
}

/**
 * Authenticates + fetches mailbox
 */
void getMailbox(const HttpRequest& request, HttpResponse& response) {
	// Assume authenticateRequest returns an optional userID if authentication is successful

	// Fetch mailbox content for the authenticated user
	// getMailboxContent is a placeholder for your method to fetch mailbox data
	auto mailboxContent = getMailboxContent(*userId);

	if (mailboxContent.empty()) {
		// No mailbox content found or some error occurred

		//fill in response stuff

		return;
	}

	// Successfully retrieved mailbox content
	response.status_code = 200;// OK
	response.reason_phrase = "OK";
	response.headers["Content-Type"] = "application/json";
	// Convert mailboxContent to JSON string if not already
	response.body = mailboxContentToJson(mailboxContent);
}


void mailbox_handler(const HttpRequest &request, HttpResponse &response) {
if (request.req_method == "GET") {
	getMailbox(request, response);
} else {
	// Method Not Allowed
}
}

int main() {
	//front end runs http server
	//create an instance of the HTTP server
	HttpServer server();

	//add routes to routing table
	server.getRoute();

	//run
	server.run();

	//end of handler --> http server sends response back to client


// Example: Dispatch a thread that contacts the load balancer here
// std::thread lbThread(contactLoadBalancer);

	//thread level socket that services 1 client

	//each socket
	//send fd command to utility function that takes a socket

	//active session tokens

//lbThread.join(); // Wait for the server thread to finish

return 0;
}
