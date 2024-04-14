#include <iostream>
#include <sys/socket.h>   // recv
#include <unistd.h>       // close

#include "../include/kvs_client.h"
#include "../include/tablet.h"
#include "../../utils/include/utils.h"

Logger kvs_client_logger("KVS Client");

// initialize constant values
// ! confirm that this is the delimiter we're looking for
const char KVSClient::delimiter = '\b';
const std::string KVSClient::ok = "+OK ";
const std::string KVSClient::err = "-ER ";

void KVSClient::read_from_network()
{
    std::vector<char> client_stream;
    uint32_t bytes_left_in_command = 0;

    int bytes_recvd;
    while (true) {
        char buf[4096];
        bytes_recvd = recv(client_fd, buf, 4096, 0);
        if (bytes_recvd < 0) {
            kvs_client_logger.log("Error reading from client", 40);
            break;
        } else if (bytes_recvd == 0) {
            kvs_client_logger.log("Client closed connection", 30);
            break;
        }

        for (int i = 0 ; i < bytes_recvd ; i++) {
            // command is not complete, append byte to client_stream and decrement number of bytes left to read to complete command
            if (bytes_left_in_command != 0) {
                client_stream.push_back(buf[i]);
                bytes_left_in_command--;
            } 
            // command is complete, we need the next 4 bytes to determine how much bytes are in this command
            else {
                // now we have two situations
                // 1) client stream's size < 4, in which case the bytes left in the command is 0 only because the prev command was completed
                // 2) client stream's size >= 4
                if (client_stream.size() < 4) {
                    client_stream.push_back(buf[i]);

                    // parse size of command once client stream is 4 bytes long and store it in 
                    if (client_stream.size() == 4) {
                        // ! note that this assumes little endian, which may be incorrect
                        // parse size of command from client
                        std::memcpy(&bytes_left_in_command, client_stream.data(), sizeof(uint32_t));
                        // clear the client stream is preparation for the incoming data
                        client_stream.clear();
                    }
                } 
                // bytes left in command is 0 because command is complete
                else {
                    handle_command(client_stream);
                    // clear the client stream in preparation for the next command
                    client_stream.clear();
                    // reset bytes left in comand to 0
                    bytes_left_in_command = 0;
                }
            }
        }
    }
    close(client_fd);
}


void KVSClient::handle_command(std::vector<char>& client_stream)
{
    // extract command from first 4 bytes
    std::string command(client_stream.begin(), client_stream.begin() + 4);
    // remove first 5 bytes to remove delimiter after command
    client_stream.erase(client_stream.begin(), client_stream.begin() + 4);
    kvs_client_logger.log("Command received - " + command, 20);

    // convert command to lowercase to standardize command
    command = Utils::to_lowercase(command);
    if (command == "getr") { getr(client_stream); }
    else if (command == "getv") { getv(client_stream);}
    else if (command == "putv") { putv(client_stream);}
    else if (command == "cput") { cput(client_stream);}
    else if (command == "delr") { delr(client_stream);}
    else if (command == "delv") { delv(client_stream);}
    else {
        kvs_client_logger.log("Unsupported command", 20);
        // send response msg
        std::string res_str = "-ER Unsupported command";
        std::vector<char> res_bytes(res_str.begin(), res_str.end());
        send_response(res_bytes);
    }
}


// ! add method to figure out which tablet to read from



void KVSClient::getr(std::vector<char>& inputs)
{
    std::string row(inputs.begin(), inputs.end());

    // ! figure out which tablet you're reading from
    Tablet tablet;

    std::vector<std::string> result = tablet.get_row(row);

    std::vector<char> response_msg;
    
    // append "+OK<SP>" and then the rest of the message
    response_msg.insert(response_msg.end(), ok.begin(), ok.end());

    // iterate through each column in response and append chars from col to response msg
    for (const std::string& col : result) {
        response_msg.insert(response_msg.end(), col.begin(), col.end());
        // insert delimiter
        response_msg.push_back(KVSClient::delimiter);
    }
    // remove last added delimiter
    response_msg.pop_back();

    // send response msg to client
    send_response(response_msg);
}


void KVSClient::getv(std::vector<char>& inputs)
{
    // split vector on delimiter
    

}


void KVSClient::putv(std::vector<char>& inputs)
{

}


void KVSClient::cput(std::vector<char>& inputs)
{

}


void KVSClient::send_response(std::vector<char>& response_msg) 
{
    // set size of response in first 4 bytes of vector
    uint32_t msg_size = static_cast<uint32_t>(response_msg.size());

    // Create a buffer for the size (4 bytes)
    std::vector<uint8_t> size_buf(sizeof(uint32_t));
    // Copy the size into the buffer in little-endian format
    // ! check this again
    size_buf[0] = static_cast<uint8_t>((msg_size >> 0) & 0xFF);
    size_buf[1] = static_cast<uint8_t>((msg_size >> 8) & 0xFF);
    size_buf[2] = static_cast<uint8_t>((msg_size >> 16) & 0xFF);
    size_buf[3] = static_cast<uint8_t>((msg_size >> 24) & 0xFF);

    // Insert the size buffer at the beginning of the original data vector
    response_msg.insert(response_msg.begin(), size_buf.begin(), size_buf.end());

    // write response to client as bytes
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < response_msg.size()) {
        int bytes_sent = send(client_fd, response_msg.data() + total_bytes_sent, response_msg.size() - total_bytes_sent, 0);
        total_bytes_sent += bytes_sent;
    }

    kvs_client_logger.log("Response sent to client", 20);
}
