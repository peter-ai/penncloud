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
#include <netdb.h> //email relay

class SMTPClient
{
private:
    // pertain to the SMTP server that your SMTP client connects to for sending emails
    // static std::string serverIP; // Static server IP: IP address of the SMTP server to which your client connects
    // static int serverPort;       // Static server Port: stores the port number on which the SMTP server is listening

    static bool starts_with(const std::string &fullString, const std::string &starting);

    // Sends data to the SMTP server using the established socket (sock).
    static void send_data(const std::string &data);

    // Receives data from the SMTP server
    static std::string receive_data();

    // DNS LookUp
    static std::string getMXRecord(const std::string &domain);

    // Resolves a hostname (typically obtained from MX records) to an IP address using
    //static std::string resolveMXtoIP(const std::string &mxHostname);
    static std::vector<std::string> resolveMXtoIP(const std::string &mxHostname);


    // static bool start_connection(const std::string &serverIP, int serverPort);
    static bool start_connection(const std::vector<std::string> &serverIPs, int serverPort);


    SMTPClient() {}

    ~SMTPClient()
    {
        close(sock);
    }

public:
    static int sock;             // between your SMTP client and the SMTP server. It represents the live connection stream through which your client sends and receives data

    static bool sendEmail(const std::string &recipientEmail, const std::string &domain, const EmailData &email);
};