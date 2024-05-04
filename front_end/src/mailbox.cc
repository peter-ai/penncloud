#include "../include/mailbox.h"

using namespace std;

struct EmailData
{
	string UIDL;
	string time;
	string to;
	string from;
	string subject;
	string body;
	string oldBody;
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
		data.oldBody = emailContents.substr(forwardPos);
	}
	return data;
}

vector<char> charifyEmailContent(const EmailData &email){
	string data = email.time + "\n" 
	+ email.from + "\n"
	+ email.to + "\n"
	+ email.subject + "\n"
	+ email.body + "\n"
	+ email.oldBody + "\n";
	std::vector<char> char_vector(data.begin(), data.end());
	return char_vector;

}

// Function to split a vector<char> based on a vector<char> delimiter
std::vector<std::vector<char>> split_vector_mbox(const std::vector<char> &data, const std::vector<char> &delimiter)
{
	std::vector<std::vector<char>> result;
	size_t start = 0;
	size_t end = data.size();

	while (start < end)
	{
		// Find the next occurrence of delimiter starting from 'start'
		auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

		if (pos == data.end())
		{
			// No delimiter found, copy the rest of the vector
			result.emplace_back(data.begin() + start, data.end());
			break;
		}
		else
		{
			// Delimiter found, copy up to the delimiter and move 'start' past the delimiter
			result.emplace_back(data.begin() + start, pos);
			start = std::distance(data.begin(), pos) + delimiter.size();
		}
	}

	return result;
}

// Function to split a vector<char> based on the first occurrence of a vector<char> delimiter
std::vector<std::vector<char>> split_vec_first_delim_mbox(const std::vector<char> &data, const std::vector<char> &delimiter)
{
	std::vector<std::vector<char>> result;
	size_t start = 0;

	// Find the first occurrence of delimiter in data
	auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

	if (pos == data.end())
	{
		// No delimiter found, return the whole vector as a single part
		result.emplace_back(data.begin(), data.end());
	}
	else
	{
		// Delimiter found, split at the delimiter
		result.emplace_back(data.begin() + start, pos);			 // Part before the delimiter
		result.emplace_back(pos + delimiter.size(), data.end()); // Part after the delimiter
	}

	return result;
}

EmailData parseEmailFromMailForm(const HttpRequest &req)
{
	EmailData emailData;

	// Check if the request contains a body
	if (!req.body_as_bytes().empty())
	{

		// Find form boundary
		std::vector<std::string> headers = req.get_header("Content-Type"); // retrieve content-type header
		std::string header_str(headers[0]);

		// boundary provided by form
		std::vector<std::string> find_boundary = Utils::split_on_first_delim(header_str, "boundary=");
		std::vector<char> bound_vec(find_boundary.back().begin(), find_boundary.back().end());

		std::vector<char> line_feed = {'\r', '\n'};

		// split body on boundary
		std::vector<std::vector<char>> parts = split_vector_mbox(req.body_as_bytes(), bound_vec);

		// Skip the first and the last part as they are the boundary preamble and closing
		for (size_t i = 1; i < parts.size() - 1; ++i)
		{

			std::vector<char> double_line_feed = {'\r', '\n', '\r', '\n'}; // Correct delimiter for headers and body separation

			std::vector<std::vector<char>> header_and_body = split_vec_first_delim_mbox(parts[i], double_line_feed);

			if (header_and_body.size() < 2)
				continue; // In case of any parsing error

			std::string headers(header_and_body[0].begin(), header_and_body[0].end());
			std::string body(header_and_body[1].begin(), header_and_body[1].end());

			headers = Utils::trim(headers);
			body = Utils::trim(body);

			// cout << "Header:" << headers << endl;
			// cout << "Bodyy:" << body << endl;

			// Finding the name attribute in the headers
			auto name_pos = headers.find("name=");

			if (name_pos != std::string::npos)
			{
				size_t start = name_pos + 6; // Skip 'name="'

				size_t end = headers.find('"', start);
				std::string name = headers.substr(start, end - start);

				// cout << name << endl;

				// Store the corresponding value in the correct field
				if (name == "time")
				{
					emailData.time = "time: " + body;
				}
				else if (name == "from")
				{
					emailData.from = "from: " + body;
				}
				else if (name == "to")
				{
					emailData.to = "to: " + body;
				}
				else if (name == "subject")
				{
					emailData.subject = "subject: " + body;
				}
				else if (name == "body")
				{
					emailData.body = "body: " + body;
				}
				else if (name == "oldBody"){
					emailData.oldBody = "oldBody: " + body;
				}
			}
		}
	}
		std::cout << "Time: " << emailData.time << std::endl;

	std::cout << "To: " << emailData.to << std::endl;
	std::cout << "From: " << emailData.from << std::endl;
	std::cout << "Subject: " << emailData.subject << std::endl;
	std::cout << "Body: " << emailData.body << std::endl;
		std::cout << "Old Body: " << emailData.oldBody << std::endl;



	return emailData;
}

/**
 * Helper functions
 */

// takes the a path's request and parses it to user mailbox key "user1-mailbox/"
string parseMailboxPathToRowKey(const string &path)
{
	std::regex pattern("/api/([^/]*)");
	std::smatch matches;

	// Execute the regex search
	if (std::regex_search(path, matches, pattern))
	{
		if (matches.size() > 1)
		{										   // Check if there is a capturing group
			return matches[1].str() + "-mailbox/"; // Return the captured username
		}
	}

	return ""; // Return empty string if no username is found
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
// forward: UIDL of message that we are forwarding

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

		// Erase the prefix "+OK " from the vector
		kvsResponse.erase(kvsResponse.begin(), kvsResponse.begin() + prefix.size());
		EmailData storedEmail = parseEmail(kvsResponse);
		// CASE 1: forwarding an email that has not been forwarded yet

		EmailData emailToForward = parseEmailFromMailForm(request);
		vector<string> recipientsEmails = parseRecipients(emailToForward.to);
		for (string recipientEmail : recipientsEmails)
		{
			string colKey = emailToForward.time + "\r" + emailToForward.from + "\r" + emailToForward.to + "\r" + emailToForward.subject;
			vector<char> col(colKey.begin(), colKey.end());
			string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
			vector<char> row(rowKey.begin(), rowKey.end());
			vector<char> value = charifyEmailContent(emailToForward);
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
	vector<char> row(rowKey.begin(), rowKey.end());
	vector<char> col(colKey.begin(), colKey.end());

	// kvs response
	vector<char> kvsGetResponse = FeUtils::kv_get(socket_fd, row, col);

	if (startsWith(kvsGetResponse, "+OK "))
	{
		const string prefix = "+OK ";

		// Erase the prefix "+OK " from the vector
		kvsGetResponse.erase(kvsGetResponse.begin(), kvsGetResponse.begin() + prefix.size());
		EmailData storedEmail = parseEmail(kvsGetResponse);
		EmailData emailResponse = parseEmailFromMailForm(request);
		vector<string> recipientsEmails = parseRecipients(emailResponse.to);
		for (string recipientEmail : recipientsEmails)
		{
			string colKey = emailResponse.time + "\r" + emailResponse.from + "\r" + emailResponse.to + "\r" + emailResponse.subject;
			vector<char> col(colKey.begin(), colKey.end());
			string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
			vector<char> row(rowKey.begin(), rowKey.end());
			vector<char> value = charifyEmailContent(emailResponse);
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
	// else couldn't find first email
	else
	{
		response.append_body_str("-ER Error processing request.");
		response.set_code(400); // Bad request
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
void sendEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	EmailData email = parseEmailFromMailForm(request);

	// recipients
	vector<string> recipientsEmails = parseRecipients(email.to);

	for (string recipientEmail : recipientsEmails)
	{
		string colKey = email.time + "\r" + email.from + "\r" + email.to + "\r" + email.subject;
		string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
		vector<char> value = charifyEmailContent(email);
		vector<char> row(rowKey.begin(), rowKey.end());
		vector<char> col(colKey.begin(), colKey.end());
		vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);

		if (kvsResponse.empty() && !startsWith(kvsResponse, "+OK"))
		{
			response.set_code(501); // Internal Server Error
			response.append_body_str("-ER Failed to send email.");
			break;
		}
	}
	response.set_code(200); // Success
	response.append_body_str("+OK Email sent successfully.");
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
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

	// string recipientAddress;

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