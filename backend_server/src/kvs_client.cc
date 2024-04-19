#include <poll.h>

#include "../include/kvs_client.h"
#include "../utils/include/be_utils.h"

Logger kvs_client_logger("KVS Client");

void KVSClient::read_from_network()
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
            kvs_client_logger.log("Client closed connection", 30);
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
                        // parse size of command from client
                        uint32_t client_stream_size;
                        std::memcpy(&client_stream_size, client_stream.data(), sizeof(uint32_t));
                        // convert number received from network order to host order
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

void KVSClient::handle_command(std::vector<char> &client_stream)
{
    // extract command from first 4 bytes
    std::string command(client_stream.begin(), client_stream.begin() + 4);
    // remove first 5 bytes to remove delimiter after command
    client_stream.erase(client_stream.begin(), client_stream.begin() + 5);
    // convert command to lowercase to standardize command
    command = Utils::to_lowercase(command);

    // SOME COMMANDS ARE KVS COMMANDS

    // SOME COMMANDS COME FROM THE PRIMARY
    // as the secondary, your job is to look for the

    if (command == "getr")
    {
        getr(client_stream);
    }
    else if (command == "getv")
    {
        getv(client_stream);
    }
    else if (command == "putv")
    {
        putv(client_stream);
    }
    else if (command == "cput")
    {
        cput(client_stream);
    }
    else if (command == "delr")
    {
        delr(client_stream);
    }
    else if (command == "delv")
    {
        delv(client_stream);
    }
    else
    {
        kvs_client_logger.log("Unsupported command", 40);
        // send error response msg
        std::string res_str = "-ER Unsupported command";
        std::vector<char> res_bytes(res_str.begin(), res_str.end());
        send_response(res_bytes);
    }
}

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

void KVSClient::getr(std::vector<char> &inputs)
{
    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // log command and args
    kvs_client_logger.log("GETR R[" + row + "]", 20);

    // retrieve tablet and read row
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->get_row(row);

    // log client response since all values are string values
    kvs_client_logger.log("GETR Response - " + std::string(response_msg.begin(), response_msg.end()), 20);

    // send response msg to client
    send_response(response_msg);
}

void KVSClient::getv(std::vector<char> &inputs)
{
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
        send_response(res_bytes);
        return;
    }
    std::string row = getv_args.substr(0, col_index);
    std::string col = getv_args.substr(col_index + 1);

    // log command and args
    kvs_client_logger.log("GETV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and read value from row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->get_value(row, col);

    // send response msg to client
    send_response(response_msg);
}

void KVSClient::forward_to_primary(std::vector<char> &inputs)
{
    kvs_client_logger.log("Forwarding operation to primary", 20);

    // forward operation to primary and wait
    BeUtils::write(BackendServer::primary_fd, inputs);
    // wait for primary to respond
    BeUtils::read(BackendServer::primary_fd);

    // ! send response to client - can send whatever the primary responded with
}

int KVSClient::send_operation_to_secondaries(std::vector<char> &inputs)
{
    // primary server - forward this message to all of your secondaries

    // send operation to each secondary
    kvs_client_logger.log("Sending operation to all secondaries", 20);
    for (int secondary : BackendServer::secondary_fds)
    {
        BeUtils::write(BackendServer::primary_fd, inputs);
    }

    kvs_client_logger.log("Waiting for secondary ACKs", 20);
    // error occurred while waiting for secondary acknowledgement of operation
    if (wait_for_secondary_acks() < 0)
    {
        kvs_client_logger.log("One or more secondaries did not send an ACK - command failed", 20);
        // ! how do we handle this situation? shouldn't we tell the secondaries to revert back?
        // ! or should we get all the acks, then respond with ok to actually finish the write?
        // ! maybe add another error message here
        return -1;
    }

    // all acks received
    // ! read all of the file descriptors and ensure they were able to perform the put correctly
    // ! if so, you can complete the write yourself

    kvs_client_logger.log("All servers acknowledged operation - completing operation on primary", 20);
}

// Poll secondaries and wait for acknowledgement from all secondaries (with timeout)
// ! double check this logic
int KVSClient::wait_for_secondary_acks()
{
    std::vector<pollfd> poll_secondary_fds(BackendServer::secondary_fds.size());
    for (size_t i = 0; i < poll_secondary_fds.size(); i++)
    {
        poll_secondary_fds[i].fd = BackendServer::secondary_fds[i];
        poll_secondary_fds[i].events = POLLIN;
        poll_secondary_fds[i].revents = 0;
    }

    // ! wait for 2 seconds in total for all secondary servers to respond
    int timeout_ms = 2000;
    struct timespec startTime, currentTime;
    clock_gettime(CLOCK_MONOTONIC, &startTime);

    int total_ready = 0;
    while (true)
    {
        // Calculate elapsed time since start
        clock_gettime(CLOCK_MONOTONIC, &currentTime);
        int elapsedMs = (currentTime.tv_sec - startTime.tv_sec) * 1000 +
                        (currentTime.tv_nsec - startTime.tv_nsec) / 1000000;

        // Poll to wait for events on secondary file descriptors with updated timeout
        int num_ready = poll(poll_secondary_fds.data(), poll_secondary_fds.size(), timeout_ms - elapsedMs);

        // file descriptors ready, add the number that were ready for reading on this iteration
        if (num_ready > 0)
        {
            // update total ready by the number ready to read on this iteration
            total_ready += num_ready;
            // check if all fds are ready to read from
            if (total_ready == poll_secondary_fds.size())
            {
                return 0;
            }
        }
        // Either timeout occurred, or error occurred during poll - failure in both cases
        else
        {
            return -1;
        }
    }
}

void KVSClient::putv(std::vector<char> &inputs)
{
    // ! Regardless of what server this is, first validate the command
    // ! If the command was invalid, the secondary could have just figured that out with initiating extra communication

    // ! maybe check if we should be doing this in this order, mabye just fire the command from the beginning

    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to PUT(R,C,V) - row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row(inputs.begin(), row_end);

    // find index of \b to extract col from inputs
    auto col_end = std::find(row_end + 1, inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to PUT(R,C,V) - column not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string col(row_end + 1, col_end);

    // remainder of input is value
    std::vector<char> val(col_end + 1, inputs.end());

    // log command and args
    kvs_client_logger.log("PUTV R[" + row + "] C[" + col + "]", 20);

    // ! we've performed basic validation on the command

    // if this server is a secondary, it needs to forward the message to its primary and wait
    // ! second condition checks that the current client is not the primary
    if (!BackendServer::is_primary && ___________)
    {
        forward_to_primary(inputs);
        return;
    }

    // sending operation to secondaries failed, don't want to continue past this
    if (send_operation_to_secondaries(inputs) < 0)
    {
        // ! maybe add an error log
        return;
    }

    // Perform the update if the backend server is a primary OR if this is a secondary and client's address is the primary's port number
    // In the second case, that means the primary wanted the secondary to perform the write and then respond

    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->put_value(row, col, val);

    // send response msg to client (+OK)
    // note that the client could be the primary that told the secondary to ini
    // The client can be one of three groups
    // 1. The secondary that forwarded the command
    // 2. The frontend server that send the command
    // 3. The primary that asked the write to be completed
    send_response(response_msg);
}

void KVSClient::delr(std::vector<char> &inputs)
{
    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // log command and args
    kvs_client_logger.log("DELR R[" + row + "]", 20);

    // retrieve tablet and delete row
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_row(row);

    // send response msg to client
    send_response(response_msg);
}

void KVSClient::delv(std::vector<char> &inputs)
{
    // convert vector to string since row and column are string-compatible values and split on delimiter
    std::string delv_args(inputs.begin(), inputs.end());

    size_t col_index = delv_args.find_first_of('\b');
    // delimiter not found in string - should be present to split row and column
    if (col_index == std::string::npos)
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to DELV(R,C) - delimiter after row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row = delv_args.substr(0, col_index);
    std::string col = delv_args.substr(col_index + 1);

    // log command and args
    kvs_client_logger.log("DELV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_value(row, col);

    // send response msg to client
    send_response(response_msg);
}

void KVSClient::cput(std::vector<char> &inputs)
{
    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to CPUT(R,C,V1,V2) - row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string row(inputs.begin(), row_end);

    // find index of \b to extract col from inputs
    auto col_end = std::find(row_end + 1, inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to CPUT(R,C,V1,V2) - column not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        send_response(res_bytes);
        return;
    }
    std::string col(row_end + 1, col_end);

    // clear inputs UP TO AND INCLUDING the last \b chara
    inputs.erase(inputs.begin(), col_end + 1);

    // remainder of input is value1 and value2

    // extract the number in front of val1
    uint32_t val1_size;
    std::memcpy(&val1_size, inputs.data(), sizeof(uint32_t));
    // convert number received from network order to host order
    int bytes_in_val1 = ntohl(val1_size);

    // clear the first 4 bytes from inputs
    inputs.erase(inputs.begin(), inputs.begin() + sizeof(uint32_t));

    // copy the number of characters in bytes_in_val1 to val1
    std::vector<char> val1;
    std::memcpy(&val1, inputs.data(), bytes_in_val1);

    // remaining characters are val2
    inputs.erase(inputs.begin(), inputs.begin() + bytes_in_val1);
    std::vector<char> val2 = inputs;

    // log command and args
    kvs_client_logger.log("CPUT R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and call CPUT on tablet
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->cond_put_value(row, col, val1, val2);

    // send response msg to client
    send_response(response_msg);
}

// ! might be duplicated in BEUtils, check this
void KVSClient::send_response(std::vector<char> &response_msg)
{
    // set size of response in first 4 bytes of vector
    // convert to network order and interpret msg_size as bytes
    uint32_t msg_size = htonl(response_msg.size());
    std::vector<uint8_t> size_prefix(sizeof(uint32_t));
    // Copy bytes from msg_size into the size_prefix vector
    std::memcpy(size_prefix.data(), &msg_size, sizeof(uint32_t));

    // Insert the size prefix at the beginning of the original response msg vector
    response_msg.insert(response_msg.begin(), size_prefix.begin(), size_prefix.end());

    // write response to client as bytes
    size_t total_bytes_sent = 0;
    while (total_bytes_sent < response_msg.size())
    {
        int bytes_sent = send(client_fd, response_msg.data() + total_bytes_sent, response_msg.size() - total_bytes_sent, 0);
        total_bytes_sent += bytes_sent;
    }
    kvs_client_logger.log("Response sent to client", 20);
}