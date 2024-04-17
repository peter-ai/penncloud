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
#include <openssl/md5.h>
#include <iomanip>


using namespace std;

struct EmailData
{
	string UIDL;
	string time;
	string to;
	string from;
	string subject;
	string body;
	string forwardedMessage; // This will hold the UIDLs of the email we are trying to forward
};

std::string parseEmailField(const std::string &emailContents, const std::string &field, size_t limit)
{
	size_t startPos = emailContents.find(field);
	if (startPos != std::string::npos && startPos < limit)
	{
		startPos += field.length(); // Move past the field name and colon
		size_t endPos = emailContents.find('\n', startPos);
		if (endPos != std::string::npos && endPos < limit)
		{
			return emailContents.substr(startPos, endPos - startPos);
		}
	}
	return "";
}

EmailData parseEmail(const std::vector<char> &source)
{
	std::string emailContents(source.begin(), source.end());
	size_t forwardPos = emailContents.find("---------- Forwarded message ---------");
	size_t limit = (forwardPos != std::string::npos) ? forwardPos : emailContents.length();

	EmailData data;
	data.time = parseEmailField(emailContents, "time: ", limit);
	data.to = parseEmailField(emailContents, "to: ", limit);
	data.from = parseEmailField(emailContents, "from: ", limit);
	data.subject = parseEmailField(emailContents, "subject: ", limit);
	data.body = parseEmailField(emailContents, "body: ", limit);

	if (forwardPos != std::string::npos)
	{
		// If there is a forwarded message, capture it
		data.forwardedMessage = emailContents.substr(forwardPos);
	}
	return data;
}
/**
 * Helper functions
 */

void computeDigest(char *data, int dataLengthBytes,
				   unsigned char *digestBuffer)
{
	/* The digest will be written to digestBuffer, which must be at least MD5_DIGEST_LENGTH bytes long */
	MD5_CTX c;
	MD5_Init(&c);
	MD5_Update(&c, data, dataLengthBytes);
	MD5_Final(digestBuffer, &c);
}

// compute unique hash for email ID
string computeEmailMD5(const string &emailText)
{
	unsigned char digestBuffer[MD5_DIGEST_LENGTH];

	computeDigest(const_cast<char *>(emailText.data()), emailText.length(),
				  digestBuffer);

	stringstream hexStream;

	hexStream << hex << std::setfill('0');
	for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
	{
		hexStream << std::setw(2) << (unsigned int)digestBuffer[i];
	}
	return hexStream.str();
}

// takes the a path's request and parses it to user mailbox key "user1-mailbox/"
string parseMailboxPathToRowKey(const string &path)
{
    std::regex pattern("/api/([^/]*)");
    std::smatch matches;

    // Execute the regex search
    if (std::regex_search(path, matches, pattern)) {
        if (matches.size() > 1) {  // Check if there is a capturing group
            return matches[1].str() + "-mailbox/";  // Return the captured username
        }
    }

    return "";  // Return empty string if no username is found
}

string extractUsernameFromEmailAddress(const string &emailAddress)
{
	size_t atPosition = emailAddress.find('@');
	if (atPosition == std::string::npos)
	{
		std::cerr << "Invalid email address." << std::endl;
		// Exit the program with an error code
	}
	else
	{
		// Extract the username part
		string username = emailAddress.substr(0, atPosition);
		return username;
	}
}

// when sending, responding to, and forwarding an email
vector<string> parseRecipients(const string &recipients)
{
	vector<std::string> result;
	istringstream ss(recipients);
	string recipient;

	while (getline(ss, recipient, ','))
	{
		recipient.erase(remove_if(recipient.begin(), recipient.end(), ::isspace), recipient.end()); // Trim spaces
		result.push_back(recipient);
	}

	return result;
}

bool startsWith(const std::vector<char> &vec, const std::string &prefix)
{
	if (vec.size() < prefix.size())
		return false;
	return std::string(vec.begin(), vec.begin() + prefix.size()) == prefix;
}

// retrieves query parameter from a request's path /api/user/mailbox/send?uidl=12345
// std::string get_query_parameter(const string& path, const std::string &key)
// {
// 	std::unordered_map<std::string, std::string> query_params;
// 	size_t queryStart = path.find('?');
// 	if (queryStart != std::string::npos)
// 	{
// 		std::string queryString = path.substr(queryStart + 1);
// 		std::istringstream queryStream(queryString);
// 		std::string param;
// 		while (std::getline(queryStream, param, '&'))
// 		{
// 			size_t equals = param.find('=');
// 			if (equals != std::string::npos)
// 			{
// 				std::string param_key = param.substr(0, equals);
// 				std::string param_value = param.substr(equals + 1);
// 				query_params[param_key] = param_value;
// 			}
// 		}
// 	}
// 	// Attempt to find the key in the parsed query parameters
// 	auto it = query_params.find(key);
// 	if (it != query_params.end())
// 	{
// 		return it->second;
// 	}
// 	return ""; // Return empty string if key is not found
// }

vector<char> modifyForwardedEmail(vector<char> emailData)
{
}

/**
 * HANDLERS
 */

// UIDL: time, to, from, subject

// EMAIL FORMAT //

// time: Fri Mar 15 18:47:23 2024\n
// to: recipient@example.com\n
// from: sender@example.com\n
// subject: Your Subject Here\n
// body: Hello, this is the body of the email.\n
// response: UIDL of message we are responding to
// forward: UIDL of message that we are forwarding
// ---------- Forwarded message ---------
// time: Fri Mar 15 18:47:23 2024\n
// to: recipient@example.com\n
// from: sender@example.com\n
// subject: Your Subject Here\n
// body: Hello, this is the body of the email.\n

// time: Fri Mar 15 18:47:23 2024\n
// to: recipient@example.com\n
// from: sender@example.com\n
// subject: Your Subject Here\n
// body: Hello, this is the body of the email.\n
// forwarded message:
// time: Fri Mar 15 18:47:23 2024\n
// to: recipient@example.com\n
// from: sender@example.com\n
// subject: Your Subject Here\n
// body: Hello, this is the body of the email.\n

void forwardEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}

	// get the email we are supposed
	string rowKey = parseMailboxPathToRowKey(request.path);
	string colKey = request.get_qparam("uidl");
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());

	// Fetch the email from KVS
	vector<char> kvsResponse = FeUtils::kv_get(socket_fd, row, col);

	if (startsWith(kvsResponse, "+OK "))
	{
		const string prefix = "+OK ";
		// Check if the vector is long enough to contain the prefix and if the prefix exists
		if (kvsResponse.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), kvsResponse.begin()))
		{
			// Erase the prefix "+OK " from the vector
			kvsResponse.erase(kvsResponse.begin(), kvsResponse.begin() + prefix.size());
			EmailData storedEmail = parseEmail(kvsResponse);
			// CASE 1: forwarding an email that has not been forwarded yet
			if (storedEmail.forwardedMessage.empty())
			{
				EmailData emailToStore = parseEmail(request.body_as_bytes());
				vector<string> recipientsEmails = parseRecipients(emailToStore.to);
				for (string recipientEmail : recipientsEmails)
				{
					string colKey = computeEmailMD5(emailToStore.time + emailToStore.from + emailToStore.to + emailToStore.subject);
					vector<char> col(colKey.begin(), colKey.end());
					string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
					vector<char> row(rowKey.begin(), rowKey.end());
					vector<char> value(emailToStore.body.begin(), emailToStore.body.end());
					vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
					if (kvsResponse.empty() && !startsWith(kvsResponse, "+OK"))
					{
						response.set_code(501); // Internal Server Error
						response.append_body_str("-ER Failed to forward email.");
						break;
					}
				}
				response.set_code(200); // Success
				response.append_body_str("+OK Email forwarded successfully.");
			}
			// CASE 2: forwarding an already forwarded email
			// subject of og email = subject of forwarded email
			// kvs response of get body append to forwarded body of email to store
			else
			{
				EmailData emailToStore = parseEmail(request.body_as_bytes());
				vector<string> recipientsEmails = parseRecipients(emailToStore.to);
				for (string recipientEmail : recipientsEmails)
				{
					string colKey = computeEmailMD5(emailToStore.time + emailToStore.from + emailToStore.to + emailToStore.subject);
					vector<char> col(colKey.begin(), colKey.end());
					string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
					vector<char> row(rowKey.begin(), rowKey.end());
					vector<char> value(emailToStore.body.begin(), emailToStore.body.end());
					vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
					if (kvsResponse.empty() && !startsWith(kvsResponse, "+OK"))
					{
						response.set_code(501); // Internal Server Error
						response.append_body_str("-ER Failed to forward email.");
						break;
					}
				}
				response.set_code(200); // Success
				response.append_body_str("+OK Email forwarded successfully.");

				// must append
			}
		}
	}
	// else couldn't find first email
	else
	{
		response.append_body_str("-ER Error processing request.");
		response.set_code(400); // Bad request
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
	string colKey = request.get_qparam("uidl");

	// prepare char vectors as arguments to kvs util
	vector<char> value_original = request.body_as_bytes();
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());

	// kvs response
	vector<char> kvsGetResponse = FeUtils::kv_get(socket_fd, row, col);

	if (!kvsGetResponse.empty() && startsWith(kvsGetResponse, "+OK"))
	{
		// we append the kvs response
		EmailData responseEmail = parseEmail(request.body_as_bytes());
		vector<string> recipientsEmails = parseRecipients(responseEmail.to);

		for (string recipientEmail : recipientsEmails)
		{
			cout << "entering recipient loop" << endl;
			string colKey = computeEmailMD5(responseEmail.time + responseEmail.from + responseEmail.to + responseEmail.subject);
			vector<char> col(colKey.begin(), colKey.end());
			string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
			vector<char> row(rowKey.begin(), rowKey.end());
			vector<char> value_response(responseEmail.body.begin(), responseEmail.body.end());

			//we also need to append the original email to the body of the response email
			value_response.insert(value_response.end(), value_original.begin(), value_original.end());

			vector<char> kvsPutResponse = FeUtils::kv_put(socket_fd, row, col, value_response);
			if (startsWith(kvsPutResponse, "-ER "))
			{
				response.set_code(501); // Internal Server Error
				response.append_body_str("-ER Failed to respond to email.");
				break;
			}
		}
		response.set_code(200); // Success
		response.append_body_str("+OK Reply sent successfully.");
	}
	else
	{
		response.set_code(404); // Internal Server Error
		response.append_body_str("-ER Failed to respond to email.");
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

// deletes an email
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
	string emailId = request.get_qparam("uidl");

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

// sends an email
void sendEMail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}

	// Parse individual parts
	EmailData email = parseEmail(request.body_as_bytes());

	// compute unique hash UIDL (column value) based on email's time, to, from, subject lines
	string colKey = computeEmailMD5(email.time + email.from + email.to + email.subject);
	string rowKey = extractUsernameFromEmailAddress(email.from) + "-mailbox/";

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
	// end of handler --> http server sends response back to client
}

// retrieves an email
void email_handler(const HttpRequest &request, HttpResponse &response)
{
	Logger logger("Email Handler");
    logger.log("Received POST request", LOGGER_INFO);
	logger.log("Path is: " + request.path, LOGGER_INFO);


	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	string rowKey = parseMailboxPathToRowKey(request.path);
	// get UIDL from path query
	string colKey = request.get_qparam("uidl");

	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());
	// Fetch the email from KVS
	logger.log("Fetching email at ROW " + rowKey + " and COLUMN " + colKey, LOGGER_DEBUG); // DEBUG
	vector<char> kvsResponse = FeUtils::kv_get(socket_fd, row, col);

	if (startsWith(kvsResponse, "+OK"))
	{
		response.set_code(200); // OK
		// get rid of "+OK "
		response.append_body_bytes(kvsResponse.data() + 4,
								   kvsResponse.size() - 4);
		logger.log("Successful response from KVS received at email handler", LOGGER_DEBUG); // DEBUG

	}
	else
	{
		response.append_body_str("-ER Error processing request.");
		response.set_code(404); // Bad request
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
	// end of handler --> http server sends response back to client
}

// gets the entire mailbox of a user
// TO DO: will need to sort according to which page is shown
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
		// get rid of "+OK "
		response.append_body_bytes(kvsResponse.data() + 4,
								   kvsResponse.size() - 4);
	}
	else
	{
		response.append_body_str("-ER Error processing request.");
		response.set_code(400); // Bad request
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
	// end of handler --> http server sends response back to client
}