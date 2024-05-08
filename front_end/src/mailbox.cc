#include "../include/mailbox.h"

using namespace std;

// Relay
string extractDomain(const string &email)
{
	size_t at_pos = email.find('@');
	string domain = email.substr(at_pos + 1);
	return domain;
}

bool isLocalDomain(const string &domain)
{
	return domain == "penncloud.com" || domain == "localhost";
}

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

// URL Decoding: creates column value based off UIDL

std::string urlEncode(const std::string &value)
{
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;

	for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
	{
		std::string::value_type c = (*i);

		// keep alphanumeric and other accepted characters as is
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			escaped << c;
			continue;
		}

		// remaining characters are percent-encoded
		escaped << std::uppercase;
		escaped << '%' << std::setw(2) << int((unsigned char)c);
		escaped << std::nouppercase;
	}

	return escaped.str();
}

// URL Decoding: takes URL value and decodes it

std::string urlDecode(const std::string &str)
{
	std::ostringstream decoded; // This will hold the decoded result

	// loop over each character in the string
	for (size_t i = 0; i < str.size(); ++i)
	{
		char c = str[i]; // get the current character

		if (c == '+')
		{ //'+' in URLs represents a space
			decoded << ' ';
		}
		else if (c == '%' && i + 2 < str.size())
		{
			// if '%' is found and there are at least two characters after it
			std::string hexValue = str.substr(i + 1, 2);		   // extract the next two characters
			int charValue;										   // store the converted hexadecimal value
			std::istringstream(hexValue) >> std::hex >> charValue; // cnvert hex to int
			decoded << static_cast<char>(charValue);			   // cast the int to char and add to the stream
			i += 2;												   // skip the next two characters that were part of the hex value
		}
		else
		{
			decoded << c; // add the character as is if it's not a special case char
		}
	}

	return decoded.str(); // convert stream to string
}

std::string get_time_and_date() {
	time_t now = time(0);
	struct tm timeStruct;
	char buf[100];
	timeStruct = *localtime(&now);
	//Mon Feb 05 23:00:00 2018
	strftime(buf, sizeof(buf), "%c", &timeStruct);
	string s = buf;
	return s;
}

// EmailData parseEmail(const std::vector<char> &source)
// {
// 	std::string emailContents(source.begin(), source.end());
// 	size_t forwardPos = emailContents.find("---------- Forwarded message ---------");
// 	size_t limit = (forwardPos != std::string::npos) ? forwardPos : emailContents.length();

// 	EmailData data;
// 	data.time = parseEmailField(emailContents, "time: ", limit);
// 	data.to = parseEmailField(emailContents, "to: ", limit);
// 	data.from = parseEmailField(emailContents, "from: ", limit);
// 	data.subject = parseEmailField(emailContents, "subject: ", limit);
// 	data.body = parseEmailField(emailContents, "body: ", limit);

// 	if (forwardPos != std::string::npos)
// 	{
// 		// If there is a forwarded message, capture it
// 		data.oldBody = emailContents.substr(forwardPos);
// 	}
// 	return data;
// }

vector<char> charifyEmailContent(const EmailData &email)
{
	string data = email.time + "\n" + email.from + "\n" + email.to + "\n" + email.subject + "\n" + email.body + "\n" + email.oldBody + "\n";
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
		std::string boundary = "--" + find_boundary.back(); // Prepend with -- to match the actual boundary
		std::vector<char> bound_vec(boundary.begin(), boundary.end());

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

			// Finding the name attribute in the headers
			auto name_pos = headers.find("name=");

			if (name_pos != std::string::npos)
			{
				size_t start = name_pos + 6; // Skip 'name="'

				size_t end = headers.find('"', start);
				std::string name = headers.substr(start, end - start);

				// Store the corresponding value in the correct field

				emailData.time = "time: " + get_time_and_date();

				if (name == "from")
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
				else if (name == "oldBody")
				{
					emailData.oldBody = "oldBody: " + body;
				}
			}
		}
	}
	// std::cout << emailData.time << std::endl;
	// std::cout << emailData.to << std::endl;
	// std::cout << emailData.from << std::endl;
	// std::cout << emailData.subject << std::endl;
	// std::cout << emailData.body << std::endl;
	// std::cout << emailData.oldBody << std::endl;

	return emailData;
}

/**
 * Helper functions
 */

// takes the a path's request and parses it to user mailbox key "user1-mailbox/"
// needs to parse both:
// /api/peter/mbox/delete?
// /peter/mbox?

string parseMailboxPathToRowKey(const string &path)
{
	std::regex pattern("/(?:api/)?([^/]+)/"); // Optionally skip 'api/' and capture the username
	std::smatch matches;

	// Execute the regex search
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
		recipient = Utils::trim(recipient);
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

void forwardEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("Failed to open socket.");
		return;
	}
	bool all_forwards_sent = true;

	EmailData emailToForward = parseEmailFromMailForm(request);
	string recipients = Utils::split_on_first_delim(emailToForward.to, ":")[1]; // parse to:peter@penncloud.com --> peter@penncloud.com
	vector<string> recipientsEmails = parseRecipients(recipients);
	for (string recipientEmail : recipientsEmails)
	{
		string recipientDomain = extractDomain(recipientEmail); // extract domain from recipient email
		// handle local client
		if (isLocalDomain(recipientDomain)) // local domain either @penncloud.com OR @localhost
		{
			string colKey = emailToForward.time + "\r" + emailToForward.from + "\r" + emailToForward.to + "\r" + emailToForward.subject;
			colKey = urlEncode(colKey); // encode UIDL in URL format for col value
			vector<char> col(colKey.begin(), colKey.end());
			string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
			vector<char> row(rowKey.begin(), rowKey.end());
			vector<char> value = charifyEmailContent(emailToForward);
			vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
			if (kvsResponse.empty() && !startsWith(kvsResponse, "+OK"))

			{
				response.set_code(501); // Internal Server Error
				response.append_body_str("-ER Failed to forward email.");
				all_forwards_sent = false;
				continue; // if one recipient fails, try to forward email to remaining recipients
			}
		}
		else
		{
			// handle external client
			// establishes a connection to the resolved IP and port, sends SMTP commands, and handles responses dynamically.
			if (!SMTPClient::sendEmail(recipientDomain, emailToForward))
			{
				response.set_code(502); // Bad Gateway
				response.append_body_str("-ER Failed to reply to email externally.");
				all_forwards_sent = false;
				continue;
			}
		}
		// check if all emails were sent
		if (all_forwards_sent)
		{
			response.set_code(200); // Success
			response.append_body_str("+OK Email sent successfully.");
		}
	}
	response.set_header("Content-Type", "text/html");
	close(socket_fd);
}

// responds to an email
void replyEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
	if (socket_fd < 0)
	{
		response.set_code(501);
		response.append_body_str("-ER Failed to open socket.");
		return;
	}
	bool all_responses_sent = true;

	EmailData emailResponse = parseEmailFromMailForm(request);
	string recipients = Utils::split_on_first_delim(emailResponse.to, ":")[1]; // parse to:peter@penncloud.com --> peter@penncloud.com
	vector<string> recipientsEmails = parseRecipients(recipients);
	for (string recipientEmail : recipientsEmails)
	{
		string recipientDomain = extractDomain(recipientEmail); // extract domain from recipient email

		// handle local client
		if (isLocalDomain(recipientDomain)) // local domain either @penncloud.com OR @localhost
		{
			string colKey = emailResponse.time + "\r" + emailResponse.from + "\r" + emailResponse.to + "\r" + emailResponse.subject;
			colKey = urlEncode(colKey); // encode UIDL in URL format for col value

			vector<char> col(colKey.begin(), colKey.end());
			string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
			vector<char> row(rowKey.begin(), rowKey.end());
			vector<char> value = charifyEmailContent(emailResponse);
			vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
			if (kvsResponse.empty() && !startsWith(kvsResponse, "+OK"))
			{
				response.set_code(501); // Internal Server Error
				response.append_body_str("-ER Failed to reply to email internally.");
				all_responses_sent = false;
				continue; // if one recipient fails, try to send response to remaining recipients
			}
		}
		else
		{
			// handle external client
			// establishes a connection to the resolved IP and port, sends SMTP commands, and handles responses dynamically.
			if (!SMTPClient::sendEmail(recipientDomain, emailResponse))
			{
				response.set_code(502); // Bad Gateway
				response.append_body_str("-ER Failed to reply to email externally.");
				all_responses_sent = false;
				continue;
			}
		}
		// check if all emails were sent
		if (all_responses_sent)
		{
			response.set_code(200); // Success
			response.append_body_str("+OK Email sent successfully.");
		}
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

	cout << "path: " << request.path << endl;

	string rowKey = parseMailboxPathToRowKey(request.path);

	cout << "row of deletion" << rowKey << endl;

	string emailId = request.get_qparam("uidl");

	cout << "email UIDL to be deleted" << emailId << endl;

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

	const EmailData email = parseEmailFromMailForm(request);		   // email data
	string recipients = Utils::split_on_first_delim(email.to, ":")[1]; // parse to:peter@penncloud.com --> peter@penncloud.com
	cout << "RECIPIENTS: " + recipients << endl;
	vector<string> recipientsEmails = parseRecipients(recipients);
	bool all_emails_sent = true;

	for (string recipientEmail : recipientsEmails)
	{
		string recipientDomain = extractDomain(recipientEmail); // extract domain from recipient email

		// handle local client
		if (isLocalDomain(recipientDomain)) // local domain either @penncloud.com OR @localhost
		{
			string colKey = email.time + "\r" + email.from + "\r" + email.to + "\r" + email.subject;
			colKey = urlEncode(colKey); // encode UIDL in URL format for col value

			string rowKey = extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
			vector<char> value = charifyEmailContent(email);
			vector<char> row(rowKey.begin(), rowKey.end());
			vector<char> col(colKey.begin(), colKey.end());
			vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);

			if (kvsResponse.empty() && !startsWith(kvsResponse, "+OK"))
			{
				response.set_code(501); // Internal Server Error
				response.append_body_str("-ER Failed to send email locally.");
				all_emails_sent = false;
				continue;
			}
		}
		else
		{
			// handle external client
			// establishes a connection to the resolved IP and port, sends SMTP commands, and handles responses dynamically.
			if (!SMTPClient::sendEmail(recipientEmail, recipientDomain, email))
			{
				response.set_code(502); // Bad Gateway
				response.append_body_str("-ER Failed to send email externally.");
				all_emails_sent = false;
				continue;
			}
		}
	}

	// check if all emails were sent
	if (all_emails_sent)
	{
		response.set_code(200); // Success
		response.append_body_str("+OK Email sent successfully.");
	}

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
	// get cookies
	unordered_map<string, string> cookies = FeUtils::parse_cookies(request);

	// check if cookies are valid!!
	if (cookies.count("user") && cookies.count("sid"))
	{
		string username = cookies["user"];
		string sid = cookies["sid"];
		bool present = HttpServer::check_kvs_addr(username);
		std::vector<std::string> kvs_addr;

		// check if we know already know the KVS server address for user
		if (present)
		{
			kvs_addr = HttpServer::get_kvs_addr(username);
		}
		// otherwise get KVS server address from coordinator
		else
		{
			// query the coordinator for the KVS server address
			kvs_addr = FeUtils::query_coordinator(username);
		}

		// create socket for communication with KVS server
		int kvs_sock = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

		// int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
		// if (socket_fd < 0)
		// {
		// 	response.set_code(501);
		// 	response.append_body_str("Failed to open socket.");
		// 	return;
		// }
		// path is: /api/mailbox/{username}/

		// string recipientAddress;

		string valid_session_id = FeUtils::validate_session_id(kvs_sock, username, request);
		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(kvs_sock);
			return;
		}

		string rowKey = parseMailboxPathToRowKey(request.path);
		vector<char> row(rowKey.begin(), rowKey.end());
		vector<char> kvsResponse = FeUtils::kv_get_row(kvs_sock, row);

		if (startsWith(kvsResponse, "+OK"))
		{
			// OK
			// get rid of "+OK "
			// response.append_body_bytes(kvsResponse.data() + 4,
			// 						   kvsResponse.size() - 4);

			// construct html page
			std::string page =
				"<!doctype html>"
				"<html lang='en' data-bs-theme='dark'>"
				"<head>"
				"<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
				"<meta content='utf-8' http-equiv='encoding'>"
				"<meta name='viewport' content='width=device-width, initial-scale=1'>"
				"<meta name='description' content='CIS 5050 Spr24'>"
				"<meta name='keywords' content='Home'>"
				"<title>Home - PennCloud.com</title>"
				"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
				"<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
				"integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
				"<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
				"<link rel='stylesheet' href='https://cdn.datatables.net/2.0.5/css/dataTables.bootstrap5.min.css' />"
				"<script src='https://cdn.datatables.net/2.0.5/js/dataTables.min.js'></script>"
				"<script src='https://cdn.datatables.net/2.0.5/js/dataTables.bootstrap5.min.js'></script>"
				"</head>"

				"<body onload='setTheme()'>"
				"<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
				"<div class='container-fluid'>"
				"<span class='navbar-brand mb-0 h1 flex-grow-1'>"
				"<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
				"class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
				"<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
				"</svg>"
				"PennCloud"
				"</span>"
				"<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup' aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
				"<span class='navbar-toggler-icon'></span>"
				"</button>"
				"<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
				"<div class='navbar-nav'>"
				"<a class='nav-link' href='/home'>Home</a>"
				"<a class='nav-link' href='/drive/" +
				username + "/'>Drive</a>"
						   "<a class='nav-link active' aria-current='page' href='/" +
				username + "/mbox'>Email</a>"
						   "<a class='nav-link disabled' aria-disabled='true'>Games</a>"
						   "<a class='nav-link' href='/account'>Account</a>"
						   "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
						   "<input type='hidden' />"
						   "<button class='btn nav-link' type='submit'>Logout</button>"
						   "</form>"
						   "</div>"
						   "</div>"
						   "<div class='form-check form-switch form-check-reverse'>"
						   "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
						   "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
						   "</div>"
						   "</div>"
						   "</nav>"

						   "<div class='container-fluid text-start'>"
						   "<div class='row mx-2 mt-3 mb-4 align-items-center'>"
						   "<div class='col-10'>"
						   "<h1 class='display-6'>Inbox</h1>"
						   "</div>"
						   "<div class='col-2 text-center'>"
						   "<button type='button' class='btn btn-primary text-center' style='width: 80%;' onclick=\"location.href='../compose'\">"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' fill='currentColor' class='bi bi-pencil-square' viewBox='0 0 16 16'>"
						   "<path d='M15.502 1.94a.5.5 0 0 1 0 .706L14.459 3.69l-2-2L13.502.646a.5.5 0 0 1 .707 0l1.293 1.293zm-1.75 2.456-2-2L4.939 9.21a.5.5 0 0 0-.121.196l-.805 2.414a.25.25 0 0 0 .316.316l2.414-.805a.5.5 0 0 0 .196-.12l6.813-6.814z'/>"
						   "<path fill-rule='evenodd' d='M1 13.5A1.5 1.5 0 0 0 2.5 15h11a1.5 1.5 0 0 0 1.5-1.5v-6a.5.5 0 0 0-1 0v6a.5.5 0 0 1-.5.5h-11a.5.5 0 0 1-.5-.5v-11a.5.5 0 0 1 .5-.5H9a.5.5 0 0 0 0-1H2.5A1.5 1.5 0 0 0 1 2.5z'/>"
						   "</svg><br/>"
						   "Compose New Email"
						   "</button>"
						   "</div>"

						   "<div class='row mx-2 mt-4 align-items-start'>"
						   "<div class='col-12'>"
						   "<table id='emailTable' class='table table-hover table-sm align-middle'>"
						   "<thead>"
						   "<tr>"
						   "<th scope='col'>Subject</th>"
						   "<th scope='col'>Time</th>"
						   "<th scope='col'>From</th>"
						   "<th scope='col'>To</th>"
						   "<th scope='col' data-dt-order='disable'></th>"
						   "<th scope='col' data-dt-order='disable'></th>"
						   "<!-- <th scope='col'></th>"
						   "<th scope='col'></th> -->"
						   "</tr>"
						   "</thead>"
						   "<tbody class='table-group-divider'>"
						   "<tr>"
						   "<th scope='row'>Some random subject 1</th>"
						   "<td>Mark</td>"
						   "<td>Otto</td>"
						   "<td>@mdo</td>"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-eye' viewBox='0 0 16 16'>"
						   "<path d='M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8M1.173 8a13 13 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5s3.879 1.168 5.168 2.457A13 13 0 0 1 14.828 8q-.086.13-.195.288c-.335.48-.83 1.12-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5s-3.879-1.168-5.168-2.457A13 13 0 0 1 1.172 8z'/>"
						   "<path d='M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5M4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0'/>"
						   "</svg>"
						   "</td>"
						   "<!-- <td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-reply-all' viewBox='0 0 16 16'>"
						   "<path d='M8.098 5.013a.144.144 0 0 1 .202.134V6.3a.5.5 0 0 0 .5.5c.667 0 2.013.005 3.3.822.984.624 1.99 1.76 2.595 3.876-1.02-.983-2.185-1.516-3.205-1.799a8.7 8.7 0 0 0-1.921-.306 7 7 0 0 0-.798.008h-.013l-.005.001h-.001L8.8 9.9l-.05-.498a.5.5 0 0 0-.45.498v1.153c0 .108-.11.176-.202.134L4.114 8.254l-.042-.028a.147.147 0 0 1 0-.252l.042-.028zM9.3 10.386q.102 0 .223.006c.434.02 1.034.086 1.7.271 1.326.368 2.896 1.202 3.94 3.08a.5.5 0 0 0 .933-.305c-.464-3.71-1.886-5.662-3.46-6.66-1.245-.79-2.527-.942-3.336-.971v-.66a1.144 1.144 0 0 0-1.767-.96l-3.994 2.94a1.147 1.147 0 0 0 0 1.946l3.994 2.94a1.144 1.144 0 0 0 1.767-.96z'/>"
						   "<path d='M5.232 4.293a.5.5 0 0 0-.7-.106L.54 7.127a1.147 1.147 0 0 0 0 1.946l3.994 2.94a.5.5 0 1 0 .593-.805L1.114 8.254l-.042-.028a.147.147 0 0 1 0-.252l.042-.028 4.012-2.954a.5.5 0 0 0 .106-.699'/>"
						   "</svg>"
						   "</td>"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-forward-fill' viewBox='0 0 16 16'>"
						   "<path d='m9.77 12.11 4.012-2.953a.647.647 0 0 0 0-1.114L9.771 5.09a.644.644 0 0 0-.971.557V6.65H2v3.9h6.8v1.003c0 .505.545.808.97.557'/>"
						   "</svg>"
						   "</td> -->"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-trash3-fill' viewBox='0 0 16 16'>"
						   "<path d='M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5m-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5M4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06m6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528M8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5'></path>"
						   "</svg>"
						   "</td>"
						   "</tr>"
						   "<tr>"
						   "<th scope='row'>Some random subject 2</th>"
						   "<td>Jacob</td>"
						   "<td>Thornton</td>"
						   "<td>@fat</td>"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-eye' viewBox='0 0 16 16'>"
						   "<path d='M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8M1.173 8a13 13 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5s3.879 1.168 5.168 2.457A13 13 0 0 1 14.828 8q-.086.13-.195.288c-.335.48-.83 1.12-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5s-3.879-1.168-5.168-2.457A13 13 0 0 1 1.172 8z'/>"
						   "<path d='M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5M4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0'/>"
						   "</svg>"
						   "</td>"
						   "<!-- <td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-reply-all' viewBox='0 0 16 16'>"
						   "<path d='M8.098 5.013a.144.144 0 0 1 .202.134V6.3a.5.5 0 0 0 .5.5c.667 0 2.013.005 3.3.822.984.624 1.99 1.76 2.595 3.876-1.02-.983-2.185-1.516-3.205-1.799a8.7 8.7 0 0 0-1.921-.306 7 7 0 0 0-.798.008h-.013l-.005.001h-.001L8.8 9.9l-.05-.498a.5.5 0 0 0-.45.498v1.153c0 .108-.11.176-.202.134L4.114 8.254l-.042-.028a.147.147 0 0 1 0-.252l.042-.028zM9.3 10.386q.102 0 .223.006c.434.02 1.034.086 1.7.271 1.326.368 2.896 1.202 3.94 3.08a.5.5 0 0 0 .933-.305c-.464-3.71-1.886-5.662-3.46-6.66-1.245-.79-2.527-.942-3.336-.971v-.66a1.144 1.144 0 0 0-1.767-.96l-3.994 2.94a1.147 1.147 0 0 0 0 1.946l3.994 2.94a1.144 1.144 0 0 0 1.767-.96z'/>"
						   "<path d='M5.232 4.293a.5.5 0 0 0-.7-.106L.54 7.127a1.147 1.147 0 0 0 0 1.946l3.994 2.94a.5.5 0 1 0 .593-.805L1.114 8.254l-.042-.028a.147.147 0 0 1 0-.252l.042-.028 4.012-2.954a.5.5 0 0 0 .106-.699'/>"
						   "</svg>"
						   "</td>"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-forward-fill' viewBox='0 0 16 16'>"
						   "<path d='m9.77 12.11 4.012-2.953a.647.647 0 0 0 0-1.114L9.771 5.09a.644.644 0 0 0-.971.557V6.65H2v3.9h6.8v1.003c0 .505.545.808.97.557'/>"
						   "</svg>"
						   "</td> -->"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-trash3-fill' viewBox='0 0 16 16'>"
						   "<path d='M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5m-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5M4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06m6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528M8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5'></path>"
						   "</svg>"
						   "</td>"
						   "</p></td>"
						   "</tr>"
						   "<tr>"
						   "<th scope='row'>Some random subject 3</th>"
						   "<td colspan='1'>Larry the Bird</td>"
						   "<td>@twitter</td>"
						   "<td>@red</td>"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-eye' viewBox='0 0 16 16'>"
						   "<path d='M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8M1.173 8a13 13 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5s3.879 1.168 5.168 2.457A13 13 0 0 1 14.828 8q-.086.13-.195.288c-.335.48-.83 1.12-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5s-3.879-1.168-5.168-2.457A13 13 0 0 1 1.172 8z'/>"
						   "<path d='M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5M4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0'/>"
						   "</svg>"
						   "</td>"
						   "<!-- <td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-reply-all' viewBox='0 0 16 16'>"
						   "<path d='M8.098 5.013a.144.144 0 0 1 .202.134V6.3a.5.5 0 0 0 .5.5c.667 0 2.013.005 3.3.822.984.624 1.99 1.76 2.595 3.876-1.02-.983-2.185-1.516-3.205-1.799a8.7 8.7 0 0 0-1.921-.306 7 7 0 0 0-.798.008h-.013l-.005.001h-.001L8.8 9.9l-.05-.498a.5.5 0 0 0-.45.498v1.153c0 .108-.11.176-.202.134L4.114 8.254l-.042-.028a.147.147 0 0 1 0-.252l.042-.028zM9.3 10.386q.102 0 .223.006c.434.02 1.034.086 1.7.271 1.326.368 2.896 1.202 3.94 3.08a.5.5 0 0 0 .933-.305c-.464-3.71-1.886-5.662-3.46-6.66-1.245-.79-2.527-.942-3.336-.971v-.66a1.144 1.144 0 0 0-1.767-.96l-3.994 2.94a1.147 1.147 0 0 0 0 1.946l3.994 2.94a1.144 1.144 0 0 0 1.767-.96z'/>"
						   "<path d='M5.232 4.293a.5.5 0 0 0-.7-.106L.54 7.127a1.147 1.147 0 0 0 0 1.946l3.994 2.94a.5.5 0 1 0 .593-.805L1.114 8.254l-.042-.028a.147.147 0 0 1 0-.252l.042-.028 4.012-2.954a.5.5 0 0 0 .106-.699'/>"
						   "</svg>"
						   "</td>"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-forward-fill' viewBox='0 0 16 16'>"
						   "<path d='m9.77 12.11 4.012-2.953a.647.647 0 0 0 0-1.114L9.771 5.09a.644.644 0 0 0-.971.557V6.65H2v3.9h6.8v1.003c0 .505.545.808.97.557'/>"
						   "</svg>"
						   "</td> -->"
						   "<td class='text-center'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-trash3-fill' viewBox='0 0 16 16'>"
						   "<path d='M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5m-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5M4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06m6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528M8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5'></path>"
						   "</svg>"
						   "</td>"
						   "</tr>"
						   "</tbody>"
						   "</table>"
						   "</div>"
						   "</div>"

						   "</div>"

						   "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
						   "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
						   "crossorigin='anonymous'></script>"
						   "<script>"
						   "$(document).ready(function(){"
						   "$('#emailTable').dataTable();"
						   "});"
						   "</script>"
						   "<script>"
						   "$('.delete').on('click', function() {"
						   "    $('#deleteModal').modal('show');"
						   "});"

						   "$('#deleteModal').on('show.bs.modal', function(e) {"
						   "let item_name = $(e.relatedTarget).attr('data-bs-name');"
						   "let file_path = $(e.relatedTarget).attr('data-bs-path');"
						   "$('#deleteModalLabel').html('Are you sure you want to delete ' + item_name + '?');"
						   "$('#deleteForm').attr('action', '/api/drive/delete/' + file_path + item_name);"
						   "});"
						   "</script>"
						   "<script>"
						   "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
						   "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
						   "document.documentElement.setAttribute('data-bs-theme', 'light');"
						   "$('#switchLabel').html('Light Mode');"
						   "sessionStorage.setItem('data-bs-theme', 'light');"
						   ""
						   "}"
						   "else {"
						   "document.documentElement.setAttribute('data-bs-theme', 'dark');"
						   "$('#switchLabel').html('Dark Mode');"
						   "sessionStorage.setItem('data-bs-theme', 'dark');"
						   "}"
						   "});"
						   "</script>"
						   "<script>"
						   "function setTheme() {"
						   "var theme = sessionStorage.getItem('data-bs-theme');"
						   "if (theme !== null) {"
						   "if (theme === 'dark') {"
						   "document.documentElement.setAttribute('data-bs-theme', 'dark');"
						   "$('#switchLabel').html('Dark Mode');"
						   "$('#flexSwitchCheckReverse').attr('checked', true);"
						   "}"
						   "else {"
						   "document.documentElement.setAttribute('data-bs-theme', 'light');"
						   "$('#switchLabel').html('Light Mode');"
						   "$('#flexSwitchCheckReverse').attr('checked', false);"
						   "}"
						   "}"
						   "};"
						   "</script>"
						   "</body>"
						   "</html>";
			response.set_code(200);
			response.append_body_str(page);
			response.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
			response.set_header("Pragma", "no-cache");
			response.set_header("Expires", "0");
			FeUtils::set_cookies(response, username, sid);
		}
		else
		{
			// response.append_body_str("-ER Error processing request.");
			// response.set_code(400); // Bad request

			// @PETER added
			response.set_code(303);
			response.set_header("Location", "/400");
			FeUtils::expire_cookies(response, username, sid);
		}

		// response.set_header("Content-Type", "text/html");
		close(kvs_sock);
	}
	else
	{
		response.set_code(303);
		response.set_header("Location", "/401");
	}

	// end of handler --> http server sends response back to client
}

void compose_email(const HttpRequest &request, HttpResponse &response)
{
	Logger logger("Compose Email");
	logger.log("Checking details", LOGGER_INFO);

	// get cookies
	std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(request);
	if (cookies.count("user") && cookies.count("sid"))
	{
		FeUtils::set_cookies(response, cookies["user"], cookies["sid"]);

		std::string page =
			"<!doctype html>"
			"<html lang='en' data-bs-theme='dark'>"
			"<head>"
			"<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
			"<meta content='utf-8' http-equiv='encoding'>"
			"<meta name='viewport' content='width=device-width, initial-scale=1'>"
			"<meta name='description' content='CIS 5050 Spr24'>"
			"<meta name='keywords' content='SignUp'>"
			"<title>Sign Up - PennCloud.com</title>"
			"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
			"<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
			"integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
			"<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
			"</head>"

			"<body onload='setTheme()'>"
			"<nav class='navbar navbar-expand-lg bg-body-tertiary'>"
			"<div class='container-fluid'>"
			"<span class='navbar-brand mb-0 h1 flex-grow-1'>"
			"<svg xmlns='http://www.w3.org/2000/svg' width='1.2em' height='1.2em' fill='currentColor'"
			"class='bi bi-cloud-fog2-fill' viewBox='0 0 16 16'>"
			"<path d='M8.5 3a5 5 0 0 1 4.905 4.027A3 3 0 0 1 13 13h-1.5a.5.5 0 0 0 0-1H1.05a3.5 3.5 0 0 1-.713-1H9.5a.5.5 0 0 0 0-1H.035a3.5 3.5 0 0 1 0-1H7.5a.5.5 0 0 0 0-1H.337a3.5 3.5 0 0 1 3.57-1.977A5 5 0 0 1 8.5 3' />"
			"</svg>"
			"PennCloud"
			"</span>"
			"<button class='navbar-toggler' type='button' data-bs-toggle='collapse' data-bs-target='#navbarNavAltMarkup' aria-controls='navbarNavAltMarkup' aria-expanded='false' aria-label='Toggle navigation'>"
			"<span class='navbar-toggler-icon'></span>"
			"</button>"
			"<div class='collapse navbar-collapse' id='navbarNavAltMarkup'>"
			"<div class='navbar-nav'>"
			"<a class='nav-link' href='/home'>Home</a>"
			"<a class='nav-link' href='/drive/" +
			cookies["user"] + "/'>Drive</a>"
							  "<a class='nav-link active' aria-current='page' href='/" +
			cookies["user"] + "/mbox'>Email</a>"
							  "<a class='nav-link disabled' aria-disabled='true'>Games</a>"
							  "<a class='nav-link' href='/account'>Account</a>"
							  "<form class='d-flex' role='form' method='POST' action='/api/logout'>"
							  "<input type='hidden' />"
							  "<button class='btn nav-link' type='submit'>Logout</button>"
							  "</form>"
							  "</div>"
							  "</div>"
							  "<div class='form-check form-switch form-check-reverse'>"
							  "<input class='form-check-input' type='checkbox' id='flexSwitchCheckReverse' checked>"
							  "<label class='form-check-label' for='flexSwitchCheckReverse' id='switchLabel'>Dark Mode</label>"
							  "</div>"
							  "</div>"
							  "</nav>"

							  "<div class='container-fluid'>"
							  "<div class='row mx-2 mt-3 mb-4'>"
							  "<div class='col-11'>"
							  "<h1 class='display-6'>"
							  "Compose new email"
							  "</h1>"
							  "</div>"
							  "<div class='col-1'>"
							  "<h1 class='display-6'>"
							  "<svg xmlns='http://www.w3.org/2000/svg' width='1em' height='1em' fill='currentColor' class='bi bi-check2-circle' viewBox='0 0 16 16'>"
							  "<path d='M2.5 8a5.5 5.5 0 0 1 8.25-4.764.5.5 0 0 0 .5-.866A6.5 6.5 0 1 0 14.5 8a.5.5 0 0 0-1 0 5.5 5.5 0 1 1-11 0'/>"
							  "<path d='M15.354 3.354a.5.5 0 0 0-.708-.708L8 9.293 5.354 6.646a.5.5 0 1 0-.708.708l3 3a.5.5 0 0 0 .708 0z'/>"
							  "</svg>"
							  "</h1>"
							  "</div>"
							  "</div>"
							  "<div class='row mt-2 mx-2'>"
							  "<div class='col-2'></div>"
							  "<div class='col-8'>"
							  "<form id='sendEmailForm' action='' method='POST' enctype='multipart/form-data'>"
							  "<div class='form-group'>"
							  "<div class='mb-3'>"
							  "<div class='form-floating mb-2'>"
							  "<input type='email' class='form-control' id='to' aria-describedby='emailHelp' name='emails' required multiple placeholder=''>"
							  "<label for='to' class='form-label'>To:</label>"
							  "<div id='emailHelp' class='form-text'>Separate recipients using commas</div>"
							  "</div>"
							  "<div class='form-floating mb-2'>"
							  "<input type='text' class='form-control' id='subject' name='subject' required placeholder=''>"
							  "<label for='subject' class='form-label'>Subject:</label>"
							  "</div>"
							  "<div class='form-floating'>"
							  "<textarea id='body' name='body' class='form-control' placeholder='' form='sendEmailForm' rows='15' style='height:100%;' required></textarea>"
							  "<label for='body'>Body:</label>"
							  "</div>"
							  "</div>"
							  "<div class='col-12'>"
							  "<button class='btn btn-primary text-center' style='float:right; width:15%' type='submit'>Send</button>"
							  "<button class='btn btn-secondary text-center' style='float:left; width:15%' type='button' onclick='location.href=\"../" +
			cookies["user"] + "/mbox\"'>Back</button>"
							  "</div>"
							  "</div>"
							  "</form>"
							  "</div>"
							  "<div class='col-2'></div>"
							  "</div>"
							  "</div>"

							  "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js'"
							  "integrity='sha384-YvpcrYf0tY3lHB60NNkmXc5s9fDVZLESaAA55NDzOxhy9GkcIdslK1eN7N6jIeHz'"
							  "crossorigin='anonymous'></script>"
							  "<script>"
							  "document.getElementById('flexSwitchCheckReverse').addEventListener('change', () => {"
							  "if (document.documentElement.getAttribute('data-bs-theme') === 'dark') {"
							  "document.documentElement.setAttribute('data-bs-theme', 'light');"
							  "$('#switchLabel').html('Light Mode');"
							  "sessionStorage.setItem('data-bs-theme', 'light');"
							  ""
							  "}"
							  "else {"
							  "document.documentElement.setAttribute('data-bs-theme', 'dark');"
							  "$('#switchLabel').html('Dark Mode');"
							  "sessionStorage.setItem('data-bs-theme', 'dark');"
							  "}"
							  "});"
							  "</script>"
							  "<script>"
							  "function setTheme() {"
							  "var theme = sessionStorage.getItem('data-bs-theme');"
							  "if (theme !== null) {"
							  "if (theme === 'dark') {"
							  "document.documentElement.setAttribute('data-bs-theme', 'dark');"
							  "$('#switchLabel').html('Dark Mode');"
							  "$('#flexSwitchCheckReverse').attr('checked', true);"
							  "}"
							  "else {"
							  "document.documentElement.setAttribute('data-bs-theme', 'light');"
							  "$('#switchLabel').html('Light Mode');"
							  "$('#flexSwitchCheckReverse').attr('checked', false);"
							  "}"
							  "}"
							  "};"
							  "</script>"
							  "</body>";

		response.set_code(200);
		response.append_body_str(page);
		response.set_header("Content-Type", "text/html");
		response.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
		response.set_header("Pragma", "no-cache");
		response.set_header("Expires", "0");
	}
	// unauthorized
	else
	{
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}
