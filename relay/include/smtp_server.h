#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <sstream>
#include <vector>
#include "../../utils/include/utils.h"
#include "../../front_end/utils/include/fe_utils.h"

class SMTPServer
{
private:
    static int serverPort; // File descriptor for the server's socket.
    static int server_fd;  // Port number on which the server listens for incoming connections.

    static void handle_client(int client_sock); // handles client connections from outside the PennCloud web system
    static int bind_server_socket();            // binds the server socket

    static EmailData construct_external_email(); // constructs email message for storage into KVS

    static void store_external_email(EmailData &email); // stores constructed email into

    static std::string extractEmailAddress(const std::string &recipient);

    static std::string get_time_and_date();

    static void sendResponse(int conn_fd, const std::string &message);

    // handlers
    static void parse_data(const std::string &emailContent, EmailData &email);

public:
    static void run(int port); // runs the SMTP server
};