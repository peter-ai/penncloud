#include <poll.h>

#include "../include/kvs_client.h"
#include "../utils/include/be_utils.h"
#include "../include/backend_server.h"

// @brief Read data from client and construct command to perform on KVS
void KVSClient::read_from_client()
{
    std::vector<char> client_stream;
    uint32_t bytes_left_in_command = 0;

    int bytes_recvd;
    while (true)
    {
        char buf[4096];
        bytes_recvd = recv(client_fd, buf, 4096, 0);
        if (bytes_recvd < 0)
        {
            kvs_client_logger.log("Error reading from client", 40);
            break;
        }
        else if (bytes_recvd == 0)
        {
            kvs_client_logger.log("Client closed connection", 20);
            break;
        }

        for (int i = 0; i < bytes_recvd; i++)
        {
            // command is not complete, append byte to client_stream and decrement number of bytes left to read to complete command
            if (bytes_left_in_command != 0)
            {
                client_stream.push_back(buf[i]);
                bytes_left_in_command--;

                // no bytes left in command - handle command and continue with additional bytes if sent
                if (bytes_left_in_command == 0)
                {
                    handle_command(client_stream);
                    // clear the client stream in preparation for the next command
                    client_stream.clear();
                    // reset bytes left in comand to 0
                    bytes_left_in_command = 0;
                }
            }
            // command is complete, we need the next 4 bytes to determine how much bytes are in this command
            else
            {
                // now we have two situations
                // 1) client stream's size < 4, in which case the bytes left in the command is 0 only because the prev command was completed
                // 2) client stream's size >= 4
                if (client_stream.size() < 4)
                {
                    client_stream.push_back(buf[i]);
                    // parse size of command once client stream is 4 bytes long and store it in bytes_left_in_command
                    if (client_stream.size() == 4)
                    {
                        // parse size of command from client byte stream
                        bytes_left_in_command = BeUtils::network_vector_to_host_num(client_stream);
                        // clear the client stream in preparation for the incoming data
                        client_stream.clear();
                    }
                }
            }
        }
    }
    close(client_fd);
}

// @brief Parse client command. If read operation, call corresponding handler. Otherwise, forward to primary.
void KVSClient::handle_command(std::vector<char> &client_stream)
{
    // extract command from first 4 bytes and convert command to lowercase
    std::string command(client_stream.begin(), client_stream.begin() + 4);
    command = Utils::to_lowercase(command);

    // READ commands - server can handle these commands directly without forwarding the operation to the primary
    std::vector<char> res_msg;
    if (command == "getr")
    {
        res_msg = getr(client_stream);
    }
    else if (command == "getv")
    {
        res_msg = getv(client_stream);
    }
    // all other commands are forwarded to the primary
    else
    {
        // forward operation to primary and wait for primary's response
        kvs_client_logger.log("Received " + command + " from client - forwarding operation to primary", 20);
        res_msg = forward_operation_to_primary(client_stream);
    }

    // send response back to client that initiated request
    send_response(res_msg);
    return;
}

/**
 * READ-ONLY COMMAND HANDLERS
 */

std::vector<char> KVSClient::getr(std::vector<char> &inputs)
{
    // erase command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // log command and args
    kvs_client_logger.log("GETR R[" + row + "]", 20);

    // retrieve tablet and read row
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->get_row(row);
    return response_msg;
}

std::vector<char> KVSClient::getv(std::vector<char> &inputs)
{
    // erase command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // convert vector to string since row and column are string-compatible values and split on delimiter
    std::string getv_args(inputs.begin(), inputs.end());

    size_t col_index = getv_args.find_first_of('\b');
    // delimiter not found in string - should be present to split row and column
    if (col_index == std::string::npos)
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to GETV(R,C) - delimiter after row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string row = getv_args.substr(0, col_index);
    std::string col = getv_args.substr(col_index + 1);

    // log command and args
    kvs_client_logger.log("GETV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and read value from row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->get_value(row, col);
    return response_msg;
}

/**
 * INTER-GROUP COMMUNICATION METHODS
 */

std::vector<char> KVSClient::forward_operation_to_primary(std::vector<char> &inputs)
{
    // open connection with primary and forward operation to primary
    BeUtils::open_connection(BackendServer::primary_port, BackendServer::server_sock_fd);
    BeUtils::write(BackendServer::server_sock_fd, inputs);

    // wait for primary to respond
    // ! This might have to be a poll since we should time out if the primary eventually doesn't respond
    // ! This could happen if the primary dies while performing the operation
    kvs_client_logger.log("Waiting for response from primary", 20);
    std::vector<char> primary_res = BeUtils::read(BackendServer::server_sock_fd);
    kvs_client_logger.log("Received response from primary - sending response to client", 20);
    return primary_res;
}

/**
 * TABLET OPERATION METHODS
 */

std::shared_ptr<Tablet> KVSClient::retrieve_data_tablet(std::string &row)
{
    for (int i = BackendServer::num_tablets - 1; i >= 0; i--)
    {
        // compare start of current tablet's with
        std::string tablet_start = BackendServer::server_tablets.at(i)->range_start;
        if (row >= tablet_start)
        {
            return BackendServer::server_tablets.at(i);
        }
    }
    // this should never execute
    kvs_client_logger.log("Could not find tablet for given row - this should NOT occur", 50);
    return nullptr;
}

/**
 * SEND CLIENT RESPONSE
 */

void KVSClient::send_response(std::vector<char> &response_msg)
{
    if (BeUtils::write(client_fd, response_msg) < 0)
    {
        kvs_client_logger.log("Failed to send response to client on port " + std::to_string(client_port), 20);
    }
    kvs_client_logger.log("Response sent to client on port " + std::to_string(client_port), 20);
}