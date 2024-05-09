#pragma once

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include "mailbox.h"
#include <string>
#include "mailbox.h"
#include "email_data.h"

#include <ldns/ldns.h> //email relay
#include <netdb.h>     //email relay

class SMTPClient
{
private:
    // utility function
    static bool starts_with(const std::string &fullString, const std::string &starting);

    // Sends data to the SMTP server using the established socket (sock).
    static void send_data(const std::string &data);

    // Receives data from the SMTP server
    static std::string receive_data();

    // DNS LookUp
    static std::string getMXRecord(const std::string &domain);

    // Resolves a hostname (typically obtained from MX records) to an IP address using
    static std::vector<std::string> resolveMXtoIP(const std::string &mxHostname);

    // start a connection based on the most reachable ip
    static bool start_connection(const std::vector<std::string> &serverIPs, int serverPort);

    // constructor
    SMTPClient() {}

    // destructor that closes socket
    ~SMTPClient()
    {
        close(sock);
    }

public:
    static int sock; // file descriptor representing the connection stream between an our SMTP client and an external SMTP server

    static bool sendEmail(const std::string &recipientEmail, const std::string &domain, const EmailData &email); // send email to our recipient
};