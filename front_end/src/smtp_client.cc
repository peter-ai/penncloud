#include "../include/smtp_client.h"

//initialize static field
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
std::string SMTPClient::resolveMXtoIP(const std::string &mxHostname)
{
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(mxHostname.c_str(), NULL, &hints, &res)) != 0)
    {
        smtp_client_logger.log(gai_strerror(status), 40);
        return "";
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

        // convert the IP to a string and break after the first one is found
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        break;
    }

    freeaddrinfo(res); // free the linked list

    return std::string(ipstr);
}

bool SMTPClient::start_connection(const std::string &serverIP, int serverPort)
{
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        
        smtp_client_logger.log("Socket creation error", 40);
        return false;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIP.c_str(), &serv_addr.sin_addr) <= 0)
    {
        smtp_client_logger.log("Invalid address / Address not supported", 40);
        return false;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        smtp_client_logger.log("Connection Failed", 40);
        return false;
    }
    return true;
}

bool SMTPClient::sendEmail(const std::string &domain, const EmailData &email)
{

    std::string mxRecord = getMXRecord(domain);

    if(mxRecord.empty()){
        smtp_client_logger.log("Cannot get MX Record", 40);
        return false;
    }
    std::string ip = resolveMXtoIP(mxRecord);

    if(ip.empty()){
        smtp_client_logger.log("Cannot resolve MX Record to IP address ", 40);
        return false;
    }

    int port = 587; // Default port for email submission
    // serverPort = port;
    // serverIP = ip;

    if (!start_connection(ip, port))
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
    send_data("HELO localhost\r\n");
    if (receive_data().find("250") != 0)
    {
        close(sock);
        return false;
    }

    // MAIL FROM command
    send_data("MAIL FROM:<" + email.from + ">\r\n");
    if (receive_data().find("250") != 0)
    {
        close(sock);
        return false;
    }

    // RCPT TO command
    send_data("RCPT TO:<" + email.to + ">\r\n");
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
    std::string message = "From: " + email.from + "\r\nTo: " + email.to + "\r\nSubject: " + email.subject + "\r\n\r\n" + email.body + "\r\n";
    if (!email.oldBody.empty())
    {
        message += "-----Original Message-----\r\n" + email.oldBody + "\r\n";
    }
    message += ".\r\n";

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
