#include "../include/mailbox.h"

using namespace std;

Logger mailbox_logger("Mailbox");

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

// function to split a vector<char> based on the first occurrence of a vector<char> delimiter
std::vector<std::vector<char>> split_vec_first_delim_mbox(const std::vector<char> &data, const std::vector<char> &delimiter)
{
	std::vector<std::vector<char>> result;
	size_t start = 0;

	// find the first occurrence of delimiter in data
	auto pos = std::search(data.begin() + start, data.end(), delimiter.begin(), delimiter.end());

	if (pos == data.end())
	{
		// no delimiter found, return the whole vector as a single part
		result.emplace_back(data.begin(), data.end());
	}
	else
	{
		// delimiter found, split at the delimiter
		result.emplace_back(data.begin() + start, pos);			 // part before the delimiter
		result.emplace_back(pos + delimiter.size(), data.end()); // part after the delimiter
	}

	return result;
}

EmailData parseEmailFromMailForm(const HttpRequest &req)
{
	EmailData emailData;

	// check if the request contains a body
	if (!req.body_as_bytes().empty())
	{
		// find form boundary
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

			// finding the name attribute in the headers
			auto name_pos = headers.find("name=");

			if (name_pos != std::string::npos)
			{
				size_t start = name_pos + 6; // Skip 'name="'

				size_t end = headers.find('"', start);
				std::string name = headers.substr(start, end - start);

				// store the corresponding value in the correct field

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
					emailData.to = "to: " + Utils::to_lowercase(body);
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

	return emailData;
}

string parseMailboxPathToRowKey(const string &path)
{
	std::regex pattern("/(?:api/)?([^/]+)/"); // Optionally skip 'api/' and capture the username
	std::smatch matches;

	// execute the regex search
	if (std::regex_search(path, matches, pattern))
	{
		if (matches.size() > 1)
		{
			return matches[1].str() + "-mailbox/";
		}
	}
	return ""; // Return empty string if no username is found
}

/**
 * HANDLERS
 */

void forwardEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	// parse cookies
	std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(request);

	// check cookies - if no cookies, automatically invalidate user and do not complete request
	if (cookies.count("user") && cookies.count("sid"))
	{

		std::string username = cookies["user"];
		std::string sid = cookies["sid"];

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

		int socket_fd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));
		if (socket_fd < 0)
		{
			response.set_code(303);
			response.set_header("Location", "/500");
			return;
		}
		bool all_forwards_sent = true;

		// validate session id
		string valid_session_id = FeUtils::validate_session_id(socket_fd, username, request);

		// redirect to login if invalid sid
		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(socket_fd);
			return;
		}

		EmailData emailToForward = parseEmailFromMailForm(request);
		string recipients = Utils::split_on_first_delim(emailToForward.to, ":")[1]; // parse to:peter@penncloud.com --> peter@penncloud.com
		vector<string> recipientsEmails = FeUtils::parseRecipients(recipients);
		for (string recipientEmail : recipientsEmails)
		{
			string recipientDomain = FeUtils::extractDomain(recipientEmail); // extract domain from recipient email
			// handle local client
			if (FeUtils::isLocalDomain(recipientDomain)) // local domain either @penncloud.com OR @localhost
			{
				string colKey = emailToForward.time + "\r" + emailToForward.from + "\r" + emailToForward.to + "\r" + emailToForward.subject;
				colKey = FeUtils::urlEncode(colKey); // encode UIDL in URL format for col value
				vector<char> col(colKey.begin(), colKey.end());
				string rowKey = FeUtils::extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
				vector<char> row(rowKey.begin(), rowKey.end());
				vector<char> value = FeUtils::charifyEmailContent(emailToForward);

				// check if row exists using get row to prevent from storing emails of users that don't exist
				std::vector<char> rowCheck = FeUtils::kv_get_row(socket_fd, row);
				if (!FeUtils::kv_success(rowCheck))
				{
					response.set_code(303); // Internal Server Error
					response.set_header("Location", "/500");
					all_forwards_sent = false;
					continue; // if one recipient fails, try to send response to remaining recipients
				}

				vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
				if (!FeUtils::kv_success(kvsResponse))
				{
					response.set_code(303); // Internal Server Error
					response.set_header("Location", "/500");
					all_forwards_sent = false;
					continue;
				}
			}
			else
			{
				// handle external client
				// establishes a connection to the resolved IP and port, sends SMTP commands, and handles responses dynamically.
				if (!SMTPClient::sendEmail(recipientEmail, recipientDomain, emailToForward))
				{
					response.set_code(303); // Bad Gateway
					response.set_header("Location", "/502");
					all_forwards_sent = false;
					continue;
				}
			}
			// check if all emails were sent
			if (all_forwards_sent)
			{
				response.set_code(303); // Success
				response.set_header("Location", "/" + username + "/mbox");
				FeUtils::set_cookies(response, username, valid_session_id);
			}
		}
		response.set_header("Content-Type", "text/html");
		close(socket_fd);
	}
	else
	{
		// set response status code
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

// responds to an email
void replyEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	Logger logger("Reply");

	// parse cookies
	std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(request);

	// check cookies - if no cookies, automatically invalidate user and do not complete request
	if (cookies.count("user") && cookies.count("sid"))
	{

		std::string username = cookies["user"];
		std::string sid = cookies["sid"];

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

		int socket_fd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));
		if (socket_fd < 0)
		{
			response.set_code(303);
			response.set_header("Location", "/500");
			return;
		}
		bool all_responses_sent = true;

		// validate session id
		string valid_session_id = FeUtils::validate_session_id(socket_fd, username, request);

		// redirect to login if invalid sid
		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(socket_fd);
			return;
		}

		EmailData emailResponse = parseEmailFromMailForm(request);

		string recipients = Utils::split_on_first_delim(emailResponse.to, ":")[1]; // parse to:peter@penncloud.com --> peter@penncloud.com
		vector<string> recipientsEmails = FeUtils::parseRecipients(recipients);

		for (string recipientEmail : recipientsEmails)
		{
			string recipientDomain = FeUtils::extractDomain(recipientEmail); // extract domain from recipient email

			// handle local client
			if (FeUtils::isLocalDomain(recipientDomain)) // local domain either @penncloud.com OR @localhost
			{
				string colKey = emailResponse.time + "\r" + emailResponse.from + "\r" + emailResponse.to + "\r" + emailResponse.subject;
				colKey = FeUtils::urlEncode(colKey); // encode UIDL in URL format for col value

				vector<char> col(colKey.begin(), colKey.end());
				string rowKey = FeUtils::extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
				vector<char> row(rowKey.begin(), rowKey.end());
				vector<char> value = FeUtils::charifyEmailContent(emailResponse); //

				logger.log(value.data(), LOGGER_DEBUG);

				// check if row exists using get row to prevent from storing emails of users that don't exist
				std::vector<char> rowCheck = FeUtils::kv_get_row(socket_fd, row);
				if (!FeUtils::kv_success(rowCheck))
				{
					response.set_code(303); // Internal Server Error
					response.set_header("Location", "/500");
					all_responses_sent = false;
					continue; // if one recipient fails, try to send response to remaining recipients
				}

				vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
				if (!FeUtils::kv_success(kvsResponse))
				{
					response.set_code(303); // Internal Server Error
					response.set_header("Location", "/500");
					all_responses_sent = false;
					continue; // if one recipient fails, try to send response to remaining recipients
				}
			}
			else
			{
				// handle external client
				// establishes a connection to the resolved IP and port, sends SMTP commands, and handles responses dynamically.
				if (!SMTPClient::sendEmail(recipientEmail, recipientDomain, emailResponse))
				{
					response.set_code(303); // Bad Gateway
					response.set_header("Location", "/502");
					all_responses_sent = false;
					continue;
				}
			}
			// check if all emails were sent
			if (all_responses_sent)
			{
				response.set_code(303); // Success
				response.set_header("Location", "/" + username + "/mbox");
				FeUtils::set_cookies(response, username, valid_session_id);
			}
		}
		response.set_header("Content-Type", "text/html");
		close(socket_fd);
	}
	else
	{
		// set response status code
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

// deletes an email
void deleteEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	// parse cookies
	std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(request);

	// check cookies - if no cookies, automatically invalidate user and do not complete request
	if (cookies.count("user") && cookies.count("sid"))
	{
		std::string username = cookies["user"];
		std::string sid = cookies["sid"];

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

		int socket_fd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));
		if (socket_fd < 0)
		{
			response.set_code(303);
			response.set_header("Location", "/500");
			return;
		}

		// validate session id
		string valid_session_id = FeUtils::validate_session_id(socket_fd, username, request);

		// redirect to login if invalid sid
		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(socket_fd);
			return;
		}

		// get mailbox and email ID
		string rowKey = parseMailboxPathToRowKey(request.path);
		string emailId = request.get_qparam("uidl");

		// construct row and column keys
		vector<char> row(rowKey.begin(), rowKey.end());
		vector<char> col(emailId.begin(), emailId.end());

		// perform delete operation
		vector<char> kvsResponse = FeUtils::kv_del(socket_fd, row, col);
		if (FeUtils::kv_success(kvsResponse))
		{
			response.set_code(303); // Success
			response.set_header("Location", "/" + username + "/mbox");
			FeUtils::set_cookies(response, username, valid_session_id);
		}
		else
		{
			// set response status code
			response.set_code(303);
			response.set_header("Location", "/500");
		}
		close(socket_fd);
	}
	else
	{
		// set response status code
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

// sends an email
void sendEmail_handler(const HttpRequest &request, HttpResponse &response)
{
	// parse cookies
	std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(request);

	// check cookies - if no cookies, automatically invalidate user and do not complete request
	if (cookies.count("user") && cookies.count("sid"))
	{
		std::string username = cookies["user"];
		std::string sid = cookies["sid"];

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

		int socket_fd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));
		if (socket_fd < 0)
		{
			response.set_code(303);
			response.set_header("Location", "/500");
			return;
		}

		// validate session id
		string valid_session_id = FeUtils::validate_session_id(socket_fd, username, request);

		// redirect to login if invalid sid
		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(socket_fd);
			return;
		}

		const EmailData email = parseEmailFromMailForm(request);		   // email data
		string recipients = Utils::split_on_first_delim(email.to, ":")[1]; // parse to:peter@penncloud.com --> peter@penncloud.com
		vector<string> recipientsEmails = FeUtils::parseRecipients(recipients);
		bool all_emails_sent = true;

		for (string recipientEmail : recipientsEmails)
		{
			string recipientDomain = FeUtils::extractDomain(recipientEmail); // extract domain from recipient email

			// handle local client
			if (FeUtils::isLocalDomain(recipientDomain)) // local domain either @penncloud.com OR @localhost
			{
				string colKey = email.time + "\r" + email.from + "\r" + email.to + "\r" + email.subject;
				colKey = FeUtils::urlEncode(colKey); // encode UIDL in URL format for col value

				string rowKey = FeUtils::extractUsernameFromEmailAddress(recipientEmail) + "-mailbox/";
				vector<char> value = FeUtils::charifyEmailContent(email);
				vector<char> row(rowKey.begin(), rowKey.end());
				vector<char> col(colKey.begin(), colKey.end());

				// check if row exists using get row to prevent from storing emails of users that don't exist
				std::vector<char> rowCheck = FeUtils::kv_get_row(socket_fd, row);
				if (!FeUtils::kv_success(rowCheck))
				{
					response.set_code(303); // Internal Server Error
					response.set_header("Location", "/500");
					all_emails_sent = false;
					continue; // if one recipient fails, try to send response to remaining recipients
				}

				vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);
				if (!FeUtils::kv_success(kvsResponse))
				{
					response.set_code(303); // Internal Server Error
					response.set_header("Location", "/500");
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
					response.set_code(303); // Bad Gateway
					response.set_header("Location", "/502");
					all_emails_sent = false;
					continue;
				}
			}
		}

		// check if all emails were sent
		if (all_emails_sent)
		{
			response.set_code(303); // Success
			response.set_header("Location", "/" + username + "/mbox");
			FeUtils::set_cookies(response, username, valid_session_id);
		}
		close(socket_fd);
	}
	else
	{
		// set response status code
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

/// @brief handler that retrieves an email of a specific user
/// @param req HttpRequest object
/// @param res HttpResponse object
void email_handler(const HttpRequest &request, HttpResponse &response)
{
	Logger logger("Email");

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
		int socket_fd = FeUtils::open_socket(kvs_addr[0], std::stoi(kvs_addr[1]));

		string valid_session_id = FeUtils::validate_session_id(socket_fd, username, request);
		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(socket_fd);
			return;
		}

		// get mailbox and email ID
		string rowKey = parseMailboxPathToRowKey(request.path);
		string colKey = request.get_qparam("uidl");

		// construct row and column keys
		vector<char> row(rowKey.begin(), rowKey.end());
		vector<char> col(colKey.begin(), colKey.end());

		// fetch the email from KVS
		vector<char> kvsResponse = FeUtils::kv_get(socket_fd, row, col);

		if (FeUtils::kv_success(kvsResponse))
		{
			// parse email body
			unordered_map<string, string> email = parseEmailBody(kvsResponse);

			std::string page =
				"<!doctype html>"
				"<html lang='en' data-bs-theme='dark'>"
				"<head>"
				"<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
				"<meta content='utf-8' http-equiv='encoding'>"
				"<meta name='viewport' content='width=device-width, initial-scale=1'>"
				"<meta name='description' content='CIS 5050 Spr24'>"
				"<meta name='keywords' content='SignUp'>"
				"<title>Email - PennCloud.com</title>"
				"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.7.1/jquery.min.js'></script>"
				"<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'"
				"integrity='sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH' crossorigin='anonymous'>"
				"<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css'>"
				"</head>"

				"<body onload='setTheme(); $(\"#reply\").click(setTimeout(function(){$(\"#reply\").click();$(\"#reply\").removeClass(\"active\");$(\"#submitButton\").hide();}, 5));'>"
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

						   "<div class='container-fluid'>"
						   "<div class='row mx-2 mt-3 mb-4 align-items-center'>"
						   "<div class='col-6'>"
						   "<h1 class='display-6'>"
						   "Email"
						   "</h1>"
						   "</div>"
						   "<div class='col-2'>"
						   "<button id='reply' class='mx-auto btn d-flex align-items-center justify-content-evenly' data-bs-toggle='button' type='button' style='width:80%' aria-pressed='false'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='1.5em' fill='currentColor' class='bi bi-reply-all-fill' viewBox='0 0 16 16'>"
						   "<path d='M8.021 11.9 3.453 8.62a.72.72 0 0 1 0-1.238L8.021 4.1a.716.716 0 0 1 1.079.619V6c1.5 0 6 0 7 8-2.5-4.5-7-4-7-4v1.281c0 .56-.606.898-1.079.62z'/>"
						   "<path d='M5.232 4.293a.5.5 0 0 1-.106.7L1.114 7.945l-.042.028a.147.147 0 0 0 0 .252l.042.028 4.012 2.954a.5.5 0 1 1-.593.805L.539 9.073a1.147 1.147 0 0 1 0-1.946l3.994-2.94a.5.5 0 0 1 .699.106'/>"
						   "</svg> Reply"
						   "</button>"
						   "</div>"
						   "<div class='col-2'>"
						   "<button id='forward' class='mx-auto btn d-flex align-items-center justify-content-evenly' data-bs-toggle='button' type='button' style='width:80%' aria-pressed='false'>"
						   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='1.5em' fill='currentColor' class='bi bi-reply-fill' viewBox='0 0 16 16' style='    transform: scaleX(-1);-moz-transform: scaleX(-1);-webkit-transform: scaleX(-1);-ms-transform: scaleX(-1);'>"
						   "<path d='M5.921 11.9 1.353 8.62a.72.72 0 0 1 0-1.238L5.921 4.1A.716.716 0 0 1 7 4.719V6c1.5 0 6 0 7 8-2.5-4.5-7-4-7-4v1.281c0 .56-.606.898-1.079.62z'/>"
						   "</svg> Forward"
						   "</button>"
						   "</div>"
						   "<div class='col-2'></div>"
						   "</div>"
						   "<div class='row mt-2 mx-2'>"
						   "<div class='col-2'></div>"
						   "<div class='col-8 mb-4'>"
						   "<form id='emailForm' action='' method='POST' enctype='multipart/form-data'>"
						   "<div class='form-group mb-3'>"
						   "<div class='mb-3'>"
						   "<div class='mb-3'>"
						   "<label for='time' class='form-label'>Received:</label>"
						   "<input type='text' class='form-control' id='time' name='time' value='" +
				email["time"] + "' required readonly>"
								"</div>"
								"<div class='mb-3'>"
								"<label for='from' class='form-label'>From:</label>"
								"<input type='email' class='form-control' id='from' name='from' value='" +
				email["from"] + "' multiple required readonly>"
								"</div>"
								"<div class='mb-3'>"
								"<label for='to' class='form-label'>Recipients:</label>"
								"<input type='email' class='form-control' id='to' name='to' value='" +
				email["to"] + "' multiple required readonly>"
							  "<div id='emailHelp' class='form-text'>Separate recipients using commas</div>"
							  "</div>"
							  "<div class='mb-3'>"
							  "<label for='subject' class='form-label'>Subject:</label>"
							  "<input type='text' class='form-control' id='subject' name='subject' value='" +
				email["subject"] + "' required readonly>"
								   "</div>"
								   "<div>"
								   "<label for='body' class='form-label'>Body:</label>"
								   "<textarea id='body' name='body' class='form-control' form='emailForm' rows='10' style='height:100%;' required readonly>" +
				email["body"] + "</textarea>"
								"</div>"
								"<div>"
								"<textarea style='display:none;' class='form-control' id='oldBody' name='oldBody'form='emailForm' rows='10' style='height:100%;' required readonly>"
								"Time: " +
				email["time"] + "\nFrom: " + email["from"] + "\nTo: " + email["to"] + "\nSubject: " + email["subject"] + "\n" + email["body"] + "\n---------------------------------\n" + email["oldBody"] +
				"</textarea>"
				"</div>"
				"</div>"
				"<div class='col-12 mb-2'>"
				"<button id='submitButton' class='btn btn-primary text-center' style='float:right; width:15%' type='submit' onclick='$(\"#time\").attr(\"value\", Date().toString()); $(\"#emailForm\").submit();'>Send</button>"
				"<button class='btn btn-secondary text-center' style='float:left; width:15%' type='button' onclick='location.href=\"../" +
				username + "/mbox\"'>Back</button>"
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
						   "var body=`" +
				email["body"] + "`;"
								"var from='" +
				email["from"] + "';"
								"var to='" +
				email["to"] + "';"
							  "var subject='" +
				email["subject"] + "';"
								   "var uidl='" +
				colKey + "';"
						 "$('#reply').on('click', function () {"
						 "if ($(this).hasClass('active')) {"
						 "if ($('#forward').hasClass('active')) {"
						 "$('#forward').removeClass('active');"
						 "$('#forward').attr('aria-pressed', 'false');"
						 "$('#subject').val($('#subject').val().replace('FWD: ', ''));"
						 "}"

						 "$('#submitButton').show();"
						 "$('label[for=time], input#time').hide();"
						 "$('#to').attr('readonly', false);"
						 "$('#to').val(from);"
						 "$('#subject').attr('readonly', false);"
						 "$('#subject').val('RE: ' + $('#subject').val());"
						 "$('#from').val('" +
				username + "@penncloud.com');"
						   "$('label[for=from], input#from').hide();"
						   "$('#body').attr('readonly', false);"
						   "$('#body').text('');"
						   "$('#body').addClass('mb-3');"
						   "$('#oldBody').css('display', '');"
						   "$('#emailForm').attr('action', '/api/" +
				username + "/mbox/reply?uidl=' + uidl);"
						   "}"
						   "else {"
						   "$('#submitButton').hide();"
						   "$('label[for=time], input#time').show();"
						   "$('#to').val(to);"
						   "$('#to').attr('readonly', true);"
						   "$('#subject').val(subject);"
						   "$('#subject').attr('readonly', true);"
						   "$('#from').val(from);"
						   "$('label[for=from], input#from').show();"
						   "$('#body').text(body);"
						   "$('#body').removeClass('mb-3');"
						   "$('#body').attr('readonly', true);"
						   "$('#oldBody').css('display', 'none');"
						   "$('#emailForm').attr('action', '');"
						   "}"
						   "});"

						   "$('#forward').on('click', function () {"
						   "if ($(this).hasClass('active')) {"
						   "if ($('#reply').hasClass('active')) {"
						   "$('#reply').removeClass('active');"
						   "$('#reply').attr('aria-pressed', 'false');"
						   "$('#subject').val($('#subject').val().replace('RE: ', ''));"
						   "}"

						   "$('#submitButton').show();"
						   "$('label[for=time], input#time').hide();"
						   "$('#to').attr('readonly', false);"
						   "$('#to').val('');"
						   "$('#subject').attr('readonly', false);"
						   "$('#subject').val('FWD: ' + $('#subject').val());"
						   "$('#from').val('" +
				username + "@penncloud.com');"
						   "$('label[for=from], input#from').hide();"
						   "$('#body').attr('readonly', false);"
						   "$('#body').text('');"
						   "$('#body').addClass('mb-3');"
						   "$('#oldBody').css('display', '');"
						   "$('#emailForm').attr('action', '/api/" +
				username + "/mbox/forward?uidl=' + uidl);"
						   "}"
						   "else {"
						   "$('#submitButton').hide();"
						   "$('label[for=time], input#time').show();"
						   "$('#to').val(to);"
						   "$('#to').attr('readonly', true);"
						   "$('#subject').val(subject);"
						   "$('#subject').attr('readonly', true);"
						   "$('#from').val(from);"
						   "$('label[for=from], input#from').show();"
						   "$('#body').text(body);"
						   "$('#body').removeClass('mb-3');"
						   "$('#body').attr('readonly', true);"
						   "$('#oldBody').css('display', 'none');"
						   "$('#emailForm').attr('action', '');"
						   "}"
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
						   "</body>";

			response.set_code(200);
			response.append_body_str(page);
			response.set_header("Content-Type", "text/html");
			response.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
			response.set_header("Pragma", "no-cache");
			response.set_header("Expires", "0");
			FeUtils::set_cookies(response, username, valid_session_id);
		}
		else
		{
			response.set_code(303);
			response.set_header("Location", "/404"); // Not found
		}
		close(socket_fd);
	}
	else
	{
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

/// @brief handler that displays contents of user mailbox
/// @param req HttpRequest object
/// @param res HttpResponse object
void mailbox_handler(const HttpRequest &request, HttpResponse &response)
{
	Logger logger("Mailbox");

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
		vector<char> kvs_response = FeUtils::kv_get_row(kvs_sock, row);

		if (FeUtils::kv_success(kvs_response))
		{
			// extract individual emails
			string inbox(kvs_response.begin() + 4, kvs_response.end());
			vector<string> mailbox = Utils::split(inbox, "\b");

			string table_rows = "";
			for (auto &email : mailbox)
			{
				// decode email
				string mail = FeUtils::urlDecode(email);
				vector<string> mail_items = Utils::split(mail, "\r");
				unordered_map<string, string> email_elements;
				for (auto &item : mail_items)
				{
					int split_idx = item.find(':');
					email_elements[item.substr(0, split_idx)] = (item.size() > split_idx + 2 ? item.substr(split_idx + 2) : "");
				}
				vector<string> recipients = Utils::split(email_elements.at("to"), ",");
				string recipient_list = "";
				for (auto &recipient : recipients)
					recipient_list += recipient + "<br>";
				if (recipient_list.size() > 1)
					recipient_list = recipient_list.substr(0, recipient_list.length() - 4);

				table_rows +=
					"<tr>"
					"<th scope='row'>" +
					email_elements.at("subject") + "</th>"
												   "<td>" +
					email_elements.at("time") + "</td>"
												"<td>" +
					email_elements.at("from") + "</td>"
												"<td>" +
					recipient_list + "</td>"
									 "<td class='text-center'>"
									 "<a href='/" +
					username + "/mbox?uidl=" + email + "'>"
													   "<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-eye' viewBox='0 0 16 16'>"
													   "<path d='M16 8s-3-5.5-8-5.5S0 8 0 8s3 5.5 8 5.5S16 8 16 8M1.173 8a13 13 0 0 1 1.66-2.043C4.12 4.668 5.88 3.5 8 3.5s3.879 1.168 5.168 2.457A13 13 0 0 1 14.828 8q-.086.13-.195.288c-.335.48-.83 1.12-1.465 1.755C11.879 11.332 10.119 12.5 8 12.5s-3.879-1.168-5.168-2.457A13 13 0 0 1 1.172 8z'/>"
													   "<path d='M8 5.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5M4.5 8a3.5 3.5 0 1 1 7 0 3.5 3.5 0 0 1-7 0'/>"
													   "</svg>"
													   "</a>"
													   "</td>"
													   "<td class='text-center'>"
													   "<form role='form' method='POST' action='/api/" +
					username + "/mbox/delete?uidl=" + email + "'>"
															  "<input type='hidden' name='test' value='" +
					email + "' />"
							"<button class='btn btn-outline-danger' type='submit'>"
							"<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-trash3-fill' viewBox='0 0 16 16'>"
							"<path d='M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5m-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5M4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06m6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528M8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5'></path>"
							"</svg>"
							"</button>"
							"</form>"
							"</td>"
							"</tr>";
			}

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
				"<title>Mailbox - PennCloud.com</title>"
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
						   "<th scope='col' style='width: 25%;'>Subject</th>"
						   "<th scope='col'>Time</th>"
						   "<th scope='col' style='width: 20%;'>From</th>"
						   "<th scope='col' style='width: 20%;'>To</th>"
						   "<th scope='col' data-dt-order='disable' style='width: 5%;'></th>"
						   "<th scope='col' data-dt-order='disable' style='width: 5%;'></th>"
						   "</tr>"
						   "</thead>"
						   "<tbody class='table-group-divider'>" +
				table_rows +

				"<!--<tr>"
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
				"<td class='text-center'>"
				"<svg xmlns='http://www.w3.org/2000/svg' width='1.5em' height='2em' fill='currentColor' class='bi bi-trash3-fill' viewBox='0 0 16 16'>"
				"<path d='M11 1.5v1h3.5a.5.5 0 0 1 0 1h-.538l-.853 10.66A2 2 0 0 1 11.115 16h-6.23a2 2 0 0 1-1.994-1.84L2.038 3.5H1.5a.5.5 0 0 1 0-1H5v-1A1.5 1.5 0 0 1 6.5 0h3A1.5 1.5 0 0 1 11 1.5m-5 0v1h4v-1a.5.5 0 0 0-.5-.5h-3a.5.5 0 0 0-.5.5M4.5 5.029l.5 8.5a.5.5 0 1 0 .998-.06l-.5-8.5a.5.5 0 1 0-.998.06m6.53-.528a.5.5 0 0 0-.528.47l-.5 8.5a.5.5 0 0 0 .998.058l.5-8.5a.5.5 0 0 0-.47-.528M8 4.5a.5.5 0 0 0-.5.5v8.5a.5.5 0 0 0 1 0V5a.5.5 0 0 0-.5-.5'></path>"
				"</svg>"
				"</td>"
				"</tr>-->"

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
				"$('#emailTable').dataTable({"
				"'oLanguage': {"
				"'sEmptyTable': 'Your inbox is empty'"
				"},"
				"order: [[2, 'asc']]"
				"});"
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
			response.set_code(303);
			response.set_header("Location", "/400");
			FeUtils::expire_cookies(response, username, sid);
		}
		close(kvs_sock);
	}
	else
	{
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

/// @brief handler that constructs page for email composition
/// @param req HttpRequest object
/// @param res HttpResponse object
void compose_email(const HttpRequest &request, HttpResponse &response)
{
	// get cookies
	std::unordered_map<std::string, std::string> cookies = FeUtils::parse_cookies(request);
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
		std::string valid_session_id = FeUtils::validate_session_id(kvs_sock, username, request);

		if (valid_session_id.empty())
		{
			// for now, returning code for check on postman
			response.set_code(303);
			response.set_header("Location", "/401");
			FeUtils::expire_cookies(response, username, sid);
			close(kvs_sock);
			return;
		}

		std::string page =
			"<!doctype html>"
			"<html lang='en' data-bs-theme='dark'>"
			"<head>"
			"<meta content='text/html;charset=utf-8' http-equiv='Content-Type'>"
			"<meta content='utf-8' http-equiv='encoding'>"
			"<meta name='viewport' content='width=device-width, initial-scale=1'>"
			"<meta name='description' content='CIS 5050 Spr24'>"
			"<meta name='keywords' content='SignUp'>"
			"<title>Compose Email - PennCloud.com</title>"
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
							  "<div class='col-8 mb-4'>"
							  "<form id='sendEmailForm' action='/api/" +
			cookies["user"] + "/mbox/send' method='POST' enctype='multipart/form-data'>"
							  "<div class='form-group'>"
							  "<div class='mb-3'>"
							  "<div><input type='hidden' class='form-control' id='from' name='from' value='" +
			cookies["user"] + "@penncloud.com'></div>"
							  "<div><input type='hidden' class='form-control' id='time' name='time' value=''></div>"
							  "<div class='form-floating mb-3'>"
							  "<input type='email' class='form-control' id='to' aria-describedby='emailHelp' name='to' required multiple placeholder=''>"
							  "<label for='to' class='form-label'>To:</label>"
							  "<div id='emailHelp' class='form-text'>Separate recipients using commas</div>"
							  "</div>"
							  "<div class='form-floating mb-3'>"
							  "<input type='text' class='form-control' id='subject' name='subject' required placeholder=''>"
							  "<label for='subject' class='form-label'>Subject:</label>"
							  "</div>"
							  "<div class='form-floating'>"
							  "<textarea id='body' name='body' class='form-control' placeholder='' form='sendEmailForm' rows='15' style='height:100%;' required></textarea>"
							  "<label for='body' class='form-label'>Body:</label>"
							  "</div>"
							  "<div><input type='hidden' class='form-control' id='oldBody' name='oldBody' value=''></div>"
							  "</div>"
							  "<div class='col-12'>"
							  "<button class='btn btn-primary text-center' style='float:right; width:15%' type='submit' onclick='$(\"#time\").attr(\"value\", Date().toString()); $(\"#sendEmailForm\").submit();'>Send</button>"
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
		FeUtils::set_cookies(response, username, valid_session_id);
	}
	// unauthorized
	else
	{
		response.set_code(303);
		response.set_header("Location", "/401");
	}
}

/// @brief helper function that parses email body after retrieval from KVS
/// @param kvs_response response from retrieving a valid email from KVS
/// @return reutrns an unordered map of email components
std::unordered_map<std::string, std::string> parseEmailBody(std::vector<char> kvs_response)
{
	string email(kvs_response.data() + 4);
	string time, from, to, subject, body, oldBody;
	int time_idx, from_idx, to_idx, subject_idx, body_idx, oldBody_idx;
	time_idx = email.find("time:");
	from_idx = email.find("from:");
	to_idx = email.find("to:");
	subject_idx = email.find("subject:");
	body_idx = email.find("body:");
	oldBody_idx = email.find("oldBody:");
	time = Utils::split_on_first_delim(email.substr(time_idx, from_idx), ":")[1].substr(1);
	from = Utils::split_on_first_delim(email.substr(from_idx, to_idx - from_idx), ":")[1].substr(1);
	to = Utils::split_on_first_delim(email.substr(to_idx, subject_idx - to_idx), ":")[1].substr(1);
	subject = Utils::split_on_first_delim(email.substr(subject_idx, body_idx - subject_idx), ":")[1].substr(1);
	body = Utils::split_on_first_delim(email.substr(body_idx, oldBody_idx - body_idx), ":")[1].substr(1);
	oldBody = Utils::split_on_first_delim(email.substr(oldBody_idx), ":")[1].substr(1);
	time.pop_back();
	from.pop_back();
	to.pop_back();
	subject.pop_back();
	body.pop_back();
	oldBody.pop_back();

	return unordered_map<string, string>{
		{"time", time},
		{"from", from},
		{"to", to},
		{"subject", subject},
		{"body", body},
		{"oldBody", oldBody}};
}