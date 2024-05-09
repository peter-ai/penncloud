#include "../include/smtp_client.h"

// initialize static field
int SMTPClient::sock = -1;

Logger smtp_client_logger("SMTP Client");

bool SMTPClient::starts_with(const std::string &fullString, const std::string &starting)
{
    return fullString.size() >= starting.size() &&
           fullString.compare(0, starting.size(), starting) == 0;
}

// Sends data to the SMTP server using the established socket (sock).
void SMTPClient::send_data(const std::string &data)
{
    size_t total_sent = 0;
    while (total_sent < data.size())
    {
        ssize_t sent = send(sock, data.c_str() + total_sent, data.size() - total_sent, 0);
        if (sent < 0)
        {
            throw std::runtime_error("Failed to send data");
        }
        total_sent += sent;
    }
    std::cout << "C: " << data;
}

// Receives data from the SMTP server
std::string SMTPClient::receive_data()
{
    std::string data;
    char buffer[1024];
    while (true)
    {
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0)
        {
            smtp_client_logger.log("Error reading from client", 40);
            break;
        }
        if (bytes_received == 0)
        {
            smtp_client_logger.log("Client closed connection", 40);
            break;
        }
        buffer[bytes_received] = '\0'; // Null-terminate
        data += buffer;
        if (bytes_received < sizeof(buffer) - 1)
        {
            break; // All data received
        }
    }
    std::cout << "S: " << data;
    return data;
}

// DNS LookUp
std::string SMTPClient::getMXRecord(const std::string &domain)
{
    ldns_resolver *res;
    ldns_rdf *name;
    ldns_pkt *pkt;
    ldns_rr_list *mx_rrs;
    ldns_status status;

    // Create a new resolver from /etc/resolv.conf
    status = ldns_resolver_new_frm_file(&res, NULL);
    if (status != LDNS_STATUS_OK)
    {
        smtp_client_logger.log("Failed to create DNS resolver", 40);
        return "";
    }

    // Convert the domain string to an ldns RDF
    name = ldns_dname_new_frm_str(domain.c_str());
    if (!name)
    {
        ldns_resolver_deep_free(res);
        smtp_client_logger.log("Failed to convert domain to RDF", 40);
        return "";
    }

    // Use the resolver to query for MX records
    pkt = ldns_resolver_query(res, name, LDNS_RR_TYPE_MX, LDNS_RR_CLASS_IN, LDNS_RD);
    ldns_rdf_deep_free(name);
    if (!pkt)
    {
        ldns_resolver_deep_free(res);
        smtp_client_logger.log("DNS query failed", 40);
        return "";
    }

    // Extract MX records from the answer section of the response
    mx_rrs = ldns_pkt_rr_list_by_type(pkt, LDNS_RR_TYPE_MX, LDNS_SECTION_ANSWER);
    if (!mx_rrs)
    {
        ldns_pkt_free(pkt);
        ldns_resolver_deep_free(res);
        smtp_client_logger.log("No MX records found", 40);
        return "";
    }

    // Assume we take the first MX record for simplicity
    ldns_rr *mx_record = ldns_rr_list_rr(mx_rrs, 0);
    ldns_rdf *mx_rdf = ldns_rr_rdf(mx_record, 1); // The second RDF in MX RR is the mail server
    char *mx_cstr = ldns_rdf2str(mx_rdf);

    std::string mx(mx_cstr);
    free(mx_cstr);

    ldns_rr_list_deep_free(mx_rrs);
    ldns_pkt_free(pkt);
    ldns_resolver_deep_free(res);

    return mx;
}

// Resolves a hostname (typically obtained from MX records) to an IP address using
// get the reachable one, not the first resolved one hence vector
// Handling multiple IP addresses by selecting the first reachable one involves modifying your connection setup to iterate through all resolved IP addresses until a successful connection is established.
std::vector<std::string> SMTPClient::resolveMXtoIP(const std::string &mxHostname)
{
    struct addrinfo hints, *res, *p;
    std::vector<std::string> ipAddresses;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Accept any IP version
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(mxHostname.c_str(), NULL, &hints, &res);

    if (status != 0)
    {
        smtp_client_logger.log(gai_strerror(status), 40);
        return ipAddresses; // Return an empty vector if resolution fails
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        void *addr;
        if (p->ai_family == AF_INET)
        { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else
        { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        ipAddresses.push_back(std::string(ipstr));
    }

    freeaddrinfo(res); // Free the linked list
    return ipAddresses;
}

bool SMTPClient::start_connection(const std::vector<std::string> &serverIPs, int serverPort)
{
    struct addrinfo hints, *res;

    for (const std::string &ip : serverIPs)
    {
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // Any IP version
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(ip.c_str(), std::to_string(serverPort).c_str(), &hints, &res) != 0)
        {
            smtp_client_logger.log("Failed to resolve server address for IP: " + ip, 40);
            continue; // Try the next IP
        }

        for (struct addrinfo *p = res; p != NULL; p = p->ai_next)
        {
            sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock < 0)
                continue;

            if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
            {
                freeaddrinfo(res); // Free the linked list
                return true;       // Successfully connected
            }
            close(sock); // Close the socket if connect fails
        }

        freeaddrinfo(res); // Free the linked list
    }

    smtp_client_logger.log("Connection failed: All IPs unreachable", 40);
    return false;
}

bool SMTPClient::sendEmail(const std::string &recipientEmail, const std::string &domain, const EmailData &email)
{
    std::cout << domain << std::endl;
    std::string mxRecord = getMXRecord(domain);

    if (mxRecord.empty())
    {
        smtp_client_logger.log("Cannot get MX Record", 40);
        return false;
    }

    std::vector<std::string> ips = resolveMXtoIP(mxRecord);
    if (ips.empty())
    {
        smtp_client_logger.log("No IPs resolved from MX record", 40);
        return false;
    }

    int port = 587; // Default port for email submission
    // serverPort = port;
    // serverIP = ip;

    for (auto &ip : ips)
    {
        smtp_client_logger.log("Resolved ip: " + ip, 20);
    }
    port = 25;
    if (!start_connection(ips, port))
    {
        close(sock);
        return false;
    }

    // Handshake with the server
    if (receive_data().find("220") != 0)
    {
        close(sock);
        return false;
    }

    // HELO command
    // TODO: @PA Added - Antispam techniques 
    // ****** Use the same dom
    send_data("HELO localhost\r\n");
    if (receive_data().find("250") != 0)
    {
        close(sock);
        return false;
    }

    // MAIL FROM command
    // change from @localhost or @penncloud.com to @seas.upenn.edu
    std::string senderWithAuthorizedDomain = Utils::split_on_first_delim(email.from, "@")[0] + "@seas.upenn.edu";
    
    //parse out
    //std::string to = Utils::split_on_first_delim(email.to, " ")[1];
    std::string from = Utils::split_on_first_delim(senderWithAuthorizedDomain, " ")[1];

    send_data("MAIL FROM:<" + from + ">\r\n");
    if (receive_data().find("250") != 0)
    {
        close(sock);
        return false;
    }

    // RCPT TO command
    send_data("RCPT TO:<" + recipientEmail + ">\r\n");
    if (receive_data().find("250") != 0)
    {
        close(sock);
        return false;
    }

    // DATA command
    send_data("DATA\r\n");
    if (receive_data().find("354") != 0)
    {
        return false;
    }

    // Message header and body with old message included
    std::string message = email.from + "\r\n" + email.to + "\r\n" + email.subject + "\r\n\r\n" + Utils::l_trim(Utils::split_on_first_delim(email.body, ":")[1]) + "\r\n";
    //std::string message = "From: " + email.from + "\r\nTo: " + email.to + "\r\nSubject: " + email.subject + "\r\n\r\n" + email.body + "\r\n";

    if (!email.oldBody.empty())
    {
        if (!email.oldBody.empty()){
        message += "-----Original Message-----\r\n" + Utils::split_on_first_delim(email.oldBody, ":")[1] + "\r\n";
        }
    }
    message += ".\r\n";
    
    Logger logger("SMTP TEST");
    logger.log(message, LOGGER_DEBUG);

    send_data(message);
    if (receive_data().find("250") != 0)
    {
        close(sock);
        return false;
    }

    // QUIT command
    send_data("QUIT\r\n");
    if (receive_data().find("221") != 0)
    {
        close(sock);
        return false;
    }

    return true;
}