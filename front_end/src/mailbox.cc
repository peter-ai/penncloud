#include <iostream>
#include "../../http_server/include/http_server.h"
#include "../utils/include/fe_utils.h"
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
#include <sstream>
#include <string>
#include <unordered_map>
#include <regex>

using namespace std;

/**
 * Helper functions
 */

// takes the a path's request and parses it to user mailbox key "user1-mbox/"
string parseMailboxPathToRowKey(const string& path)
{
	regex userRegex("/api/(\\w+)");
	regex mailboxRegex("/(mbox)(?:/|$)"); // Matches 'mbox' or 'mailbox' followed by '/' or end of string

	std::smatch userMatch, mailboxMatch;

	std::string username, mailbox;

	// Search for username
	if (regex_search(path, userMatch, userRegex))
	{
		if (userMatch.size() > 1)
		{
			username = userMatch[1].str();
		}
	}

	// Search for mailbox or mbox
	if (regex_search(path, mailboxMatch, mailboxRegex))
	{
		if (mailboxMatch.size() > 1)
		{
			mailbox = mailboxMatch[1].str();
		}
	}

	if (!username.empty() && !mailbox.empty())
	{
		return username + "-" + mailbox + "/";
	}
	return "";
}

bool startsWith(const std::vector<char> &vec, const std::string &prefix)
{
	if (vec.size() < prefix.size())
		return false;
	return std::string(vec.begin(), vec.begin() + prefix.size()) == prefix;
}

// retrieves query parameter from a request's path /api/user/mailbox/send?uidl=12345
std::string get_query_parameter(const HttpRequest &request, const std::string &key)
{
	std::unordered_map<std::string, std::string> query_params;
	size_t queryStart = request.path.find('?');
	if (queryStart != std::string::npos)
	{
		std::string queryString = request.path.substr(queryStart + 1);
		std::istringstream queryStream(queryString);
		std::string param;
		while (std::getline(queryStream, param, '&'))
		{
			size_t equals = param.find('=');
			if (equals != std::string::npos)
			{
				std::string param_key = param.substr(0, equals);
				std::string param_value = param.substr(equals + 1);
				query_params[param_key] = param_value;
			}
		}
	}
	// Attempt to find the key in the parsed query parameters
	auto it = query_params.find(key);
	if (it != query_params.end())
	{
		return it->second;
	}
	return ""; // Return empty string if key is not found
}

vector<char> modifyForwardedEmail(vector<char> emailData)
{
}

/**
 * HANDLERS
 */

// UIDL: time, to, from, subject

// EMAIL FORMAT //
// time: Fri Mar 15 18:47:23 2024
//"to": "recipient@example.com",
//"from": "sender@example.com",
//"subject": "Your Subject Here",
//"body": "Hello, this is the body of the email."

void forwardEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}

	// Extract the email ID and destination address from the query or body
	string rowKey = parseMailboxPathToRowKey(request.path);
	string colKey = get_query_parameter(request, "uidl");
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());
	// Fetch the email from KVS
	vector<char> emailData = FeUtils::kv_get(socket_fd, row, col);

	if (emailData.empty() && startsWith(emailData, "-ER"))
	{
		response.set_code(501); // Internal Server Error
		response.append_body_str("-ER Failed to respond to email.");
	}
	else
	{
		string forwardTo = get_query_parameter(request, "forwardTo") + "-mbox/";
		vector<char> forward(forwardTo.begin(), forwardTo.end());

		// would a forwarded email have a new UIDL? In this case we need to
		vector<char> kvsResponse = FeUtils::kv_put(socket_fd, forward, col, emailData);

		if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK"))
		{
			response.set_code(200); // Success
			response.append_body_str("+OK Email forwarded successfully.");
		}
		else
		{
			response.set_code(501); // Internal Server Error
			response.append_body_str("-ER Failed to forward email.");
		}
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

void replyEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("-ER Failed to open socket.");
		return;
	}
	// get row and col key as strings
	string rowKey = parseMailboxPathToRowKey(request.path);
	string colKey = get_query_parameter(request, "uidl");

	// prepare char vectors as arguments to kvs util
	vector<char> value = request.body_as_bytes();
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());

	// kvs response
	vector<char> kvsResponse = FeUtils::kv_get(socket_fd, row, col);

	if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK"))
	{
		response.set_code(200); // Success
		response.append_body_str("+OK Reply sent successfully.");
	}
	else
	{
		response.set_code(501); // Internal Server Error
		response.append_body_str("-ER Failed to respond to email.");
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

void deleteEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	string rowKey = parseMailboxPathToRowKey(request.path);
	string emailId = get_query_parameter(request, "uidl");

	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(emailId.begin(), emailId.end());

	vector<char> kvsResponse = FeUtils::kv_del(socket_fd, row, col);

	if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK"))
	{
		response.set_code(200); // Success
		response.append_body_str("+OK Email deleted successfully.");
	}
	else
	{
		response.set_code(501); // Internal Server Error
		response.append_body_str("-ER Failed to delete email.");
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

void sendEMail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	string rowKey = parseMailboxPathToRowKey(request.path);
	// this will be the UIDL of the email
	string colKey = get_query_parameter(request, "uidl");

	vector<char> value = request.body_as_bytes();
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());

	vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);

	if (!kvsResponse.empty() && startsWith(kvsResponse, "+OK"))
	{
		response.set_code(200); // Success
		response.append_body_str("+OK Email sent successfully.");
	}
	else
	{
		response.set_code(501); // Internal Server Error
		response.append_body_str("-ER Failed to send email.");
	}

	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

void email_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	string rowKey = parseMailboxPathToRowKey(request.path);
	string colKey = get_query_parameter(request, "uidl");
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());
	// Fetch the email from KVS
	vector<char> kvsResponse = FeUtils::kv_get(socket_fd, row, col);

	if (startsWith(kvsResponse, "+OK"))
	{
		response.set_code(200); // OK
		char *charPointer = kvsResponse.data();
		response.append_body_bytes(kvsResponse.data() + 3,
								   kvsResponse.size() - 3);
	}
	else
	{
		response.append_body_str("Error processing request.");
		response.set_code(400); // Bad request
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

// will need to sort according to which page is shown
void mailbox_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	// path is: /api/mailbox/{username}/
	string rowKey = parseMailboxPathToRowKey(request.path);
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> kvsResponse = FeUtils::kv_get_row(socket_fd, row);

	if (startsWith(kvsResponse, "+OK"))
	{
		response.set_code(200); // OK
		char *charPointer = kvsResponse.data();
		response.append_body_bytes(kvsResponse.data() + 3,
								   kvsResponse.size() - 3);
	}
	else
	{
		response.append_body_str("Error processing request.");
		response.set_code(400); // Bad request
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
	// end of handler --> http server sends response back to client
}