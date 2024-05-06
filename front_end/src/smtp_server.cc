//TO DELETE SINCE CLIENT USES OUR FUNCTIONALITY

// #include "../include/smtp_server.h"

// Logger smtp_server_logger("SMTP Server");

// using namespace std;

// /**
//  * Handles individual client connections in a separate thread.
//  * Parses SMTP commands and sends appropriate responses.
//  *
//  * @param client_sock Socket file descriptor for the connected client.
//  */

// void SMTPServer::handle_client(int client_sock)
// {
//     const int buffer_size = 1024;
//     char buffer[buffer_size];
//     string data_segment;

//     while (true)
//     {
//         memset(buffer, 0, buffer_size);
//         int recv_len = recv(client_sock, buffer, buffer_size - 1, 0);
//         if (recv_len <= 0)
//         {
//             if (recv_len == 0)
//             {
//                 smtp_server_logger.log("Client disconnected.", 20);
//             }
//             else
//             {
//                 smtp_server_logger.log("recv failed.", 40);
//             }
//             break;
//         }

//         smtp_server_logger.log("Received: ", 20);

//         if (strstr(buffer, "HELO") != nullptr)
//         {
//             send(client_sock, "250 Hello\r\n", 11, 0);
//         }
//         else if (strstr(buffer, "MAIL FROM:") != nullptr)
//         {
//             send(client_sock, "250 OK\r\n", 8, 0);
//         }
//         else if (strstr(buffer, "RCPT TO:") != nullptr)
//         {
//             send(client_sock, "250 OK\r\n", 8, 0);
//         }
//         else if (strstr(buffer, "DATA") != nullptr)
//         {
//             send(client_sock, "354 Start mail input\r\n", 22, 0);
//             data_segment.clear(); // Prepare to collect message data
//         }
//         else if (strstr(buffer, ".") != nullptr && !data_segment.empty())
//         {
//             // Check if the data segment ends correctly, then process it
//             send(client_sock, "250 OK\r\n", 8, 0);
//             data_segment.clear();
//         }
//         else if (strstr(buffer, "QUIT") != nullptr)
//         {
//             send(client_sock, "221 Bye\r\n", 9, 0);
//             break;
//         }
//         else
//         {
//             send(client_sock, "500 Error: command not understood\r\n", 36, 0);
//         }
//     }
//     close(client_sock);
// }

// /**
//  * @brief Binds the server socket to the specified port and starts listening for incoming connections.
//  */
// int SMTPServer::bind_server_socket()
// {
//     SMTPServer::server_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (SMTPServer::server_fd  == 0)
//     {
//         smtp_server_logger.log("Could not create socket.", 40);
//         close(SMTPServer::server_fd);
//         return -1;
//     }

//     struct sockaddr_in address;
//     address.sin_family = AF_INET;
//     address.sin_addr.s_addr = INADDR_ANY;
//     address.sin_port = htons(serverPort);


//     int opt = 1;
//     if ((setsockopt(SMTPServer::server_fd , SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) < 0)
//     {
//         smtp_server_logger.log("Unable to reuse port to bind server socket.", 40);
//         return -1;
//     }

//     if (bind(SMTPServer::server_fd , (struct sockaddr *)&address, sizeof(address)) < 0)
//     {
//         smtp_server_logger.log("Bind failed.", 40);
//         close(SMTPServer::server_fd );
//         return -1;
//     }

//     if (listen(SMTPServer::server_fd , 3) < 0)
//     {
//         smtp_server_logger.log("Listen failed.", 40);
//         close(SMTPServer::server_fd );
//         return -1;
//     }
// }

// /**
//  * @brief Initializes the server on the specified port and manages incoming connections.
//  * Each connection is handled in a separate thread to allow concurrent processing.
//  *
//  * @param port The port number on which the server should listen.
//  */
// void SMTPServer::run(int port)
// {
//     SMTPServer::serverPort = port;

//     smtp_server_logger.log("SMTP Server started. Waiting for connections....", 20);

//     if (bind_server_socket() < 0)
//     {
//         smtp_server_logger.log("Failed to initialize SMTP server. Exiting.", 40);
//         return;
//     }

//     smtp_server_logger.log("SMTP server listening for connections on port " + to_string(SMTPServer::serverPort), 20);

//     while (true)
//     {
//         struct sockaddr_in client_address;
//         int addrlen = sizeof(client_address);
//         int new_socket = accept(SMTPServer::server_fd, (struct sockaddr *)&client_address, (socklen_t *)&addrlen);
//         if (new_socket < 0)
//         {
//             smtp_server_logger.log("Unable to accept incoming connection from SMTP client. Skipping.", 30);
//             // error with incoming connection should NOT break the server loop
//             continue;
//         }
//         smtp_server_logger.log("Accepted connection from SMTP client __________", 20);
//         std::thread(handle_client, new_socket).detach();
//     }
// };