#include <iostream>
#include <sys/socket.h>   // recv
#include <unistd.h>       // close

#include "../include/kvs_client.h"
#include "../include/tablet.h"
#include "../include/backend_server.h"
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

                if (bytes_left_in_command == 0) {
                    kvs_client_logger.log("handling command", 40);
                    handle_command(client_stream);
                    // clear the client stream in preparation for the next command
                    client_stream.clear();
                    // reset bytes left in comand to 0
                    bytes_left_in_command = 0;
                }
            }
            // command is complete, we need the next 4 bytes to determine how much bytes are in this command
            else {
                // now we have two situations
                // 1) client stream's size < 4, in which case the bytes left in the command is 0 only because the prev command was completed
                // 2) client stream's size >= 4
                if (client_stream.size() < 4) {
                    client_stream.push_back(buf[i]);
                    // parse size of command once client stream is 4 bytes long and store it in bytes_left_in_command
                    if (client_stream.size() == 4) {
                        // parse size of command from client
                        uint32_t client_stream_size;
                        std::memcpy(&client_stream_size, client_stream.data(), sizeof(uint32_t));
                        // ! convert number received from network order to host order
                        bytes_left_in_command = ntohl(client_stream_size);
                        // clear the client stream in preparation for the incoming data
                        client_stream.clear();
                    }
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
    client_stream.erase(client_stream.begin(), client_stream.begin() + 5);
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
        kvs_client_logger.log("Unsupported command", 40);
        // send error response msg
        std::string res_str = KVSClient::err + "Unsupported command";
        std::vector<char> res_bytes(res_str.begin(), res_str.end());
        send_response(res_bytes);
    }
}


// ! add method to figure out which tablet to read from
std::shared_ptr<Tablet> KVSClient::retrieve_data_tablet(std::string& row)
{
    for (int i = BackendServer::num_tablets - 1 ; i >= 0 ; i--) {
        // compare start of current tablet's with 
        std::string tablet_start = BackendServer::server_tablets.at(i)->range_start;
        if (row >= tablet_start) {
            return BackendServer::server_tablets.at(i);
        }
    }
    // this should never execute
    kvs_client_logger.log("Could not find tablet for given row - this should NOT occur", 50);
    return nullptr;
}


void KVSClient::getr(std::vector<char>& inputs)
{
    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // retrieve tablet to read row from
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    // read row from tablet
    std::vector<std::string> result = tablet->get_row(row);

    // construct response msg from vector of strings containing rows in column
    std::vector<char> response_msg;
    // append "+OK<SP>" and then the rest of the message
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    // iterate through each column in response and append chars from each col to response msg
    for (const std::string& col : result) {
        response_msg.insert(response_msg.end(), col.begin(), col.end());
        // insert delimiter to separate columns
        response_msg.push_back(KVSClient::delimiter);
    }
    // remove last added delimiter
    response_msg.pop_back();

    kvs_client_logger.log(std::string(response_msg.begin(), response_msg.end()), 20);

    // send response msg to client
    send_response(response_msg);
}


void KVSClient::getv(std::vector<char>& inputs)
{
    // convert vector to string since row and column are string-compatible values and split on delimiter
    std::string getv_args(inputs.begin(), inputs.end());

    size_t col_index = getv_args.find_first_of('\b');
    // delimiter not found in string - should be present to split row and column
    if (col_index == std::string::npos) {
        // log and send error message
        std::string err_msg = KVSClient::err + "Malformed arguments to GET(R,C) - delimiter not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row = getv_args.substr(0, col_index);
    std::string col = getv_args.substr(col_index + 1);

    kvs_client_logger.log("row - " + row, 20);
    kvs_client_logger.log("col - " + col, 20);

    // retrieve tablet and read value from row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->get_value(row, col);

    kvs_client_logger.log(std::string(response_msg.begin(), response_msg.end()), 20);

    // construct response msg from value
    // append "+OK<SP>" and then the rest of the message
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());

    kvs_client_logger.log(std::string(response_msg.begin(), response_msg.end()), 20);

    // send response msg to client
    send_response(response_msg);
}


void KVSClient::putv(std::vector<char>& inputs)
{
    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end()) {
       // log and send error message
        std::string err_msg = KVSClient::err + "Malformed arguments to PUT(R,C,V) - row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row(inputs.begin(), row_end);

    // find index of \b to extract col from inputs
    auto col_end = std::find(row_end + 1, inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end()) {
       // log and send error message
        std::string err_msg = KVSClient::err + "Malformed arguments to PUT(R,C,V) - column not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string col(row_end + 1, col_end);

    // remainder of input is value
    std::vector<char> val(col_end + 1, inputs.end());

    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    tablet->put_value(row, col, val);

    // construct response msg
    // append "+OK<SP>" and then the rest of the message
    kvs_client_logger.log("+OK PUT value at r:" + row + ", c:" + col , 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    // send response msg to client
    send_response(response_msg);
}


void KVSClient::cput(std::vector<char>& inputs)
{
    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end()) {
       // log and send error message
        std::string err_msg = KVSClient::err + "Malformed arguments to CPUT(R,C,V1,V2) - row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row(inputs.begin(), row_end);

    // find index of \b to extract col from inputs
    auto col_end = std::find(row_end + 1, inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end()) {
       // log and send error message
        std::string err_msg = KVSClient::err + "Malformed arguments to CPUT(R,C,V1,V2) - column not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string col(row_end + 1, col_end);

    // remainder of input is value1 and value2
    // ! implement logic to extract value1 and value2
    std::vector<char> val1;
    std::vector<char> val2;

    // retrieve tablet and call CPUT on tablet
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    tablet->cond_put_value(row, col, val1, val2);

    // construct response msg
    // append "+OK<SP>" and then the rest of the message
    kvs_client_logger.log("+OK CPUT value at r:" + row + ", c:" + col , 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    // send response msg to client
    send_response(response_msg);
}


void KVSClient::delr(std::vector<char>& inputs)
{
    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // retrieve tablet to delete row from
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    // delete row from tablet
    tablet->delete_row(row);

    // construct response msg
    // append "+OK<SP>" and then the rest of the message
    kvs_client_logger.log("+OK DELETE row at r:" + row, 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    // send response msg to client
    send_response(response_msg);
}


void KVSClient::delv(std::vector<char>& inputs)
{
    // convert vector to string since row and column are string-compatible values and split on delimiter
    std::string delv_args(inputs.begin(), inputs.end());

    size_t col_index = delv_args.find_first_of('\b');
    // delimiter not found in string - should be present to split row and column
    if (col_index == std::string::npos) {
        // log and send error message
        std::string err_msg = KVSClient::err + "Malformed arguments to DELV(R,C) - delimiter not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row = delv_args.substr(0, col_index);
    std::string col = delv_args.substr(col_index + 1);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    tablet->delete_value(row, col);

    // construct response msg
    // append "+OK<SP>" and then the rest of the message
    kvs_client_logger.log("+OK DELETE value at r:" + row + ", c:" + col , 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    // send response msg to client
    send_response(response_msg);
}



void KVSClient::send_response(std::vector<char>& response_msg) 
{
    // set size of response in first 4 bytes of vector
    // ! convert to network order and interpret msg_size as bytes
    uint32_t msg_size = htonl(response_msg.size());
    std::vector<uint8_t> size_prefix(sizeof(uint32_t));
    // Copy bytes from msg_size into the size_prefix vector
    std::memcpy(size_prefix.data(), &msg_size, sizeof(uint32_t));

    // Insert the size prefix at the beginning of the original response msg vector
    response_msg.insert(response_msg.begin(), size_prefix.begin(), size_prefix.end());

    // write response to client as bytes
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < response_msg.size()) {
        int bytes_sent = send(client_fd, response_msg.data() + total_bytes_sent, response_msg.size() - total_bytes_sent, 0);
        total_bytes_sent += bytes_sent;
    }
    kvs_client_logger.log("Response sent to client", 20);
}