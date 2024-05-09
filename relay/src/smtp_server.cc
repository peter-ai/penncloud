#include "../include/smtp_server.h"

using namespace std;

// *********************************************
// STATIC FIELD INITIALIZATION
// *********************************************

int SMTPServer::server_fd = -1;
int SMTPServer::serverPort = -1;

// smtp server logger
Logger smtp_server_logger("SMTP Server");

/**
 * @brief helper function that sends a response
 *
 * @param conn_fd File descriptor for our server-client connection
 * @param message Message to be sent
 */
void SMTPServer::sendResponse(int conn_fd, const string &message)
{
    string formattedMessage = message + "\r\n";
    send(conn_fd, formattedMessage.c_str(), formattedMessage.length(), 0);
}

/**
 * @brief Stores parsed email into KVS.
 *
 * @param email The email data structure that is to be stored
 */
std::string SMTPServer::extractEmailAddress(const std::string &recipient)
{
    size_t start = recipient.find('<') + 1; // Position after '<'
    size_t end = recipient.rfind('>');      // Find closing '>', use rfind to ensure we get the last occurrence
    if (start == string::npos || end == string::npos || start >= end)
    {
        return recipient; // Return the original if no brackets found or if in incorrect order
    }
    return recipient.substr(start, end - start); // Extract the email address
}

/**
 * @brief Gets current time and date
 */
std::string SMTPServer::get_time_and_date()
{
    time_t now = time(0);
    struct tm timeStruct;
    char buf[100];
    timeStruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %b %-d %H:%M:%S %Y", &timeStruct); // Mon Feb 05 23:00:00 2018
    string s = buf;
    return s;
}

/**
 * @brief Stores parsed email into KVS.
 *
 * @param email The email data structure that is to be stored
 */
void SMTPServer::store_external_email(EmailData &email)
{
    // cout << "UIDL: " << email.UIDL << endl;
    //  cout << "Time: " << email.time << endl;
    //  cout << "To: " << email.to << endl;
    //  cout << "From: " << email.from << endl;
    //  cout << "Subject: " << email.subject << endl;
    //  cout << "Body: " << email.body << endl;
    //  cout << "OldBody: " << email.oldBody << endl;
    // cout << "#END OF EMAIL# " << endl;

    // add colon prefixes to email fields (important for col parsing)
    email.time = "time: " + email.time;
    email.to = "to: " + email.to;
    email.from = "from: " + email.from;
    email.subject = "subject: " + email.subject;
    email.body = "body: " + email.body;
    email.oldBody = "oldBody: " + email.oldBody;

    int socket_fd = FeUtils::open_socket(SERVADDR, SERVPORT);
    if (socket_fd < 0)
    {
        smtp_server_logger.log("Could not create KVS socket.", 40);
        close(socket_fd);
        return;
    }

    string recipients = Utils::split_on_first_delim(email.to, ":")[1]; // parse to:user@penncloud.com --> user@penncloud.com
    vector<string> recipientsEmails = FeUtils::parseRecipients(recipients);
    bool all_emails_sent = true;

    for (string recipientEmail : recipientsEmails)
    {
        cout << recipientEmail << endl;
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

            std::string rowCheckMsg = std::string(rowCheck.begin(), rowCheck.end());

            smtp_server_logger.log("Response from KVS: " + rowCheckMsg, 10);

            if (FeUtils::kv_success(rowCheck))
            {
                smtp_server_logger.log("User " + recipientEmail + " does not exist in the PennCloud system.", 40);
                all_emails_sent = false;
                continue;
            }

            smtp_server_logger.log("Storing external email in KVS for row: " + rowKey, 20);

            vector<char> kvsResponse = FeUtils::kv_put(socket_fd, row, col, value);

            if (FeUtils::kv_success(kvsResponse))
            {
                smtp_server_logger.log("Failed to store external email in KVS for row: " + rowKey, 40);
                all_emails_sent = false;
                continue;
            }
        }
    }
    if (all_emails_sent)
    {
        smtp_server_logger.log("All received emails stored in KVS successfully.", 20);
    }
    close(socket_fd);
}

/**
 * Handles individual client connections in a separate thread.
 * Parses SMTP commands and sends appropriate responses.
 *
 * @param emailContent parsed email content
 * @param email structure that separates out email content into to, from, subject, body, oldbody (reply/forward)
 */
void SMTPServer::parse_data(const std::string &emailContent, EmailData &email)
{
    std::istringstream stream(emailContent);
    std::string line;
    bool bodyFound = false;
    bool collectingOldBody = false;

    while (getline(stream, line))
    {
        // smtp_server_logger.log("#LINE#: " + line, 10);  // Log every line read
        if (!bodyFound && line.find("Subject:") != std::string::npos)
        {
            std::string subject = line.substr(line.find("Subject:") + 8);
            // smtp_server_logger.log("Subject is: " + subject, 20);
            email.subject = subject;
        }
        else if (line.find("Content-Transfer-Encoding:") != std::string::npos)
        {
            // smtp_server_logger.log("STARTING TO COLLECT BODY", 10);
            getline(stream, line); // Supposed to be an empty line
            getline(stream, line); // This should now be the first line of the body
            email.body += line + "\n";
            bodyFound = true;

            while (getline(stream, line))
            {
                if (line.find("On ") == 0 && line.find("wrote:") != std::string::npos)
                {
                    // smtp_server_logger.log("COLLECTING REPLY", 10);
                    collectingOldBody = true;
                    email.oldBody += line + "\n";
                    break;
                }
                else if (line == "-------- Forwarded Message --------")
                {
                    // smtp_server_logger.log("COLLECTING FORWARD", 10);
                    collectingOldBody = true;
                    email.oldBody += line + "\n";
                    break;
                }
                else
                {
                    // smtp_server_logger.log("COLLECTING BODY", 10);
                    email.body += line + "\n";
                }
            }

            if (collectingOldBody)
            {
                while (getline(stream, line))
                {
                    // smtp_server_logger.log("COLLECTING OLD BODY", 10);
                    email.oldBody += line + "\n";
                }
            }
        }
    }
    //  trim white space
    email.subject = Utils::trim(email.subject);
    email.body = Utils::trim(email.body);
    email.oldBody = Utils::trim(email.oldBody);
}

/**
 * Handles individual client connections.
 * Parses SMTP commands and sends appropriate responses.
 *
 * @param client_sock Socket file descriptor for the connected client.
 */
void SMTPServer::handle_client(int client_sock)
{
    const int buffer_size = 1024;
    char buffer[buffer_size];
    string commandBuffer;
    string data_segment;
    bool collectingData = false;
    EmailData email; // Construct email based on received data

    smtp_server_logger.log("Handling Client.", 20);

    // Send initial greeting message to client
    string message = "220 Service ready\r\n";
    SMTPServer::sendResponse(client_sock, message);

    while (true)
    {
        memset(buffer, 0, buffer_size);
        smtp_server_logger.log("Waiting to receive data", 20);
        int recv_len = recv(client_sock, buffer, buffer_size - 1, 0);

        if (recv_len <= 0)
        {
            if (recv_len == 0)
            {
                smtp_server_logger.log("Client disconnected.", 20);
            }
            else
            {
                smtp_server_logger.log("recv failed.", 40);
            }
            break;
        }

        // Append received data to commandBuffer
        commandBuffer.append(buffer, recv_len);

        // Process complete commands (terminated by CRLF "\r\n")
        size_t pos;
        while ((pos = commandBuffer.find("\r\n")) != string::npos)
        {
            string line = commandBuffer.substr(0, pos);
            commandBuffer.erase(0, pos + 2);

            if (collectingData)
            {
                if (line == ".")
                {
                    collectingData = false;
                    parse_data(data_segment, email); // parse subject, body, and old body
                    data_segment.clear();
                    message = "250 OK";
                    SMTPServer::sendResponse(client_sock, message);
                    break; // terminate after one email is collected
                }
                data_segment += line + "\n";
                continue;
            }

            if (line.find("HELO ") == 0 || line.find("EHLO ") == 0)
            {
                email.from.clear();
                email.to.clear();
                email.body.clear();

                email.time = get_time_and_date();
                message = "250 localhost";
                SMTPServer::sendResponse(client_sock, message);
            }
            else if (line.find("MAIL FROM:") == 0)
            {
                email.from = SMTPServer::extractEmailAddress(line.substr(10));
                message = "250 OK";
                SMTPServer::sendResponse(client_sock, message);
            }
            //"RCPT TO:" sent for each recipient
            else if (line.find("RCPT TO:") == 0)
            {
                cout << line << endl;
                email.to += SMTPServer::extractEmailAddress(line.substr(8)) + ",";
                message = "250 OK";
                SMTPServer::sendResponse(client_sock, message);
            }
            else if (line == "DATA")
            {
                collectingData = true;
                message = "354 Start mail input; end with <CRLF>.<CRLF>";
                SMTPServer::sendResponse(client_sock, message);
            }
            else if (line == "QUIT")
            {
                message = "221 Bye";
                SMTPServer::sendResponse(client_sock, message);
                break;
            }
            else
            {
                message = "500 Error: command not recognized";
                SMTPServer::sendResponse(client_sock, message);
            }
        }
    }
    store_external_email(email); // Store email in KVS
    close(client_sock);
    smtp_server_logger.log("Client socket closed", 20);
}

/**
 * @brief Binds the server socket to the specified port and starts listening for incoming connections.
 */
int SMTPServer::bind_server_socket()
{
    SMTPServer::server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (SMTPServer::server_fd < 0)
    {
        smtp_server_logger.log("Could not create socket.", 40);
        close(SMTPServer::server_fd);
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(serverPort);

    int opt = 1;
    if ((setsockopt(SMTPServer::server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0)
    {
        smtp_server_logger.log("Unable to reuse port to bind server socket.", 40);
        return -1;
    }

    if ((::bind(SMTPServer::server_fd, (const sockaddr *)&address, sizeof(address))) < 0)
    {
        smtp_server_logger.log("Bind failed.", 40);
        close(SMTPServer::server_fd);
        return -1;
    }

    if (listen(SMTPServer::server_fd, 3) < 0)
    {
        smtp_server_logger.log("Listen failed.", 40);
        close(SMTPServer::server_fd);
        return -1;
    }
}

/**
 * @brief Initializes the server on the specified port and manages incoming connections.
 * Each connection is handled in a separate thread to allow concurrent processing.
 *
 * @param port The port number on which the server should listen.
 */
void SMTPServer::run(int port)
{
    SMTPServer::serverPort = port;

    smtp_server_logger.log("SMTP Server started. Waiting for connections....", 20);

    if (bind_server_socket() < 0)
    {
        smtp_server_logger.log("Failed to initialize SMTP server. Exiting.", 40);
        return;
    }

    smtp_server_logger.log("SMTP server listening for connections on port " + to_string(SMTPServer::serverPort), 20);

    while (true)
    {
        struct sockaddr_in client_address;
        int addrlen = sizeof(client_address);
        int new_socket = accept(SMTPServer::server_fd, (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            smtp_server_logger.log("Unable to accept incoming connection from SMTP client. Skipping.", 30);
            // error with incoming connection should NOT break the server loop
            continue;
        }
        smtp_server_logger.log("Accepted connection from SMTP client __________", 20);

        std::thread client_thread(handle_client, new_socket);
        if (client_thread.joinable())
        {
            client_thread.detach();
            smtp_server_logger.log("Thread detached successfully.", 20);
        }
        else
        {
            smtp_server_logger.log("Failed to detach thread.", 40);
        }
    }
};
