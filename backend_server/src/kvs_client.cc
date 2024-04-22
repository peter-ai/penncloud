#include <poll.h>

#include "../include/kvs_client.h"
#include "../utils/include/be_utils.h"
#include "../include/backend_server.h"

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

void KVSClient::handle_command(std::vector<char> &client_stream)
{
    // extract command from first 4 bytes and convert command to lowercase
    std::string command(client_stream.begin(), client_stream.begin() + 4);
    // client_stream.erase(client_stream.begin(), client_stream.begin() + 5);
    command = Utils::to_lowercase(command);

    // validate command sent by client
    if (BackendServer::commands.count(command) == 0)
    {
        kvs_client_logger.log("Unsupported command", 40);
        // send error response msg
        std::string res_str = "-ER Unsupported command";
        std::vector<char> res_bytes(res_str.begin(), res_str.end());
        send_response(res_bytes);
        return;
    }

    // READ commands
    // These can be performed by any server - call command handler directly, send response back to client, and return
    if (command == "getr")
    {
        std::vector<char> read_res = getr(client_stream);
        send_response(read_res);
        return;
    }
    else if (command == "getv")
    {
        std::vector<char> read_res = getv(client_stream);
        send_response(read_res);
        return;
    }

    // WRITE COMMANDS
    // behavior differs depending on if this server is a primary or a secondary

    // primary server
    if (BackendServer::is_primary)
    {
        // if the operation is a secn or secy, it should update which server it received the acknowledgement from
        // ! assuming only secy for now
        if (command == "secy" || command == "secn")
        {
            // erase command from start of client stream
            client_stream.erase(client_stream.begin(), client_stream.begin() + 5);
            // extract sequence number from secondary
            int cmd_seq_num = BeUtils::network_vector_to_host_num(client_stream);
            // insert port of secondary that sent acknowledgement
            BackendServer::msg_acks_recvd.at(cmd_seq_num).insert(client_port);

            // check if the message at the front of the holdback queue has as many acknowledgements as there are secondaries
            HoldbackOperation next_operation = BackendServer::holdback_operations.top();
            while (BackendServer::msg_acks_recvd.at(next_operation.seq_num).size() == BackendServer::secondary_ports.size())
            {
                // pop this operation from secondary_holdback_operations
                BackendServer::holdback_operations.pop();
                // erase seq number acknowledgements from map
                BackendServer::msg_acks_recvd.erase(next_operation.seq_num);
                // extract command from first 4 bytes
                std::string operation_cmd(next_operation.msg.begin(), next_operation.msg.begin() + 4);
                // convert command to lowercase to standardize command
                operation_cmd = Utils::to_lowercase(operation_cmd);
                // perform write operation on primary server
                std::vector<char> response_msg = call_write_command(operation_cmd, next_operation.msg);
                // TODO this won't work - response will just go to last secondary that sent the response
                send_response(response_msg);
                // exit if no more operations left in holdback queue
                if (BackendServer::holdback_operations.size() == 0)
                {
                    break;
                }
                next_operation = BackendServer::holdback_operations.top();
            }
            return;
        }

        // send operation to all secondaries
        kvs_client_logger.log("Received " + command + " - sending operation to secondaries", 20);
        if (send_operation_to_secondaries(client_stream) < 0)
        {
            // log and send error message if failure occurred while sending operation to secondaries
            std::string err_msg = "-ER Failed to send operation to all secondaries";
            kvs_client_logger.log(err_msg, 40);
            std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
            send_response(res_bytes);
            return;
        }
    }
    // secondary server
    else
    {
        // Primary sent request asking secondary to perform write operation on secondary
        if (command == "pwrt")
        {
            kvs_client_logger.log("Primary requested write", 20);

            // erase command from beginning of inputs
            client_stream.erase(client_stream.begin(), client_stream.begin() + 5);

            // extract sequence number and erase from stream
            uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(client_stream);
            client_stream.erase(client_stream.begin(), client_stream.begin() + 4);

            // wrap operation with its sequence number and add to secondary holdback queue
            HoldbackOperation operation;
            operation.seq_num = operation_seq_num;
            operation.msg = client_stream;
            BackendServer::holdback_operations.push(operation);

            // iterate holdback queue and figure out if the command at the front is the command you want to perform next (1 + seq_num)
            HoldbackOperation next_operation = BackendServer::holdback_operations.top();
            while (next_operation.seq_num == BackendServer::seq_num + 1)
            {
                // pop this operation from secondary_holdback_operations
                BackendServer::holdback_operations.pop();
                // increment sequence number on secondary
                BackendServer::seq_num += 1;
                // extract command from first 4 bytes
                std::string operation_cmd(next_operation.msg.begin(), next_operation.msg.begin() + 4);
                // convert command to lowercase to standardize command
                operation_cmd = Utils::to_lowercase(operation_cmd);
                // perform write operation on secondary server
                std::vector<char> response_msg = call_write_command(operation_cmd, next_operation.msg);

                // ! if the operation was successful, send SECY, otherwise send SECN
                std::vector<char> primary_res_msg;
                if (response_msg.front() == '+')
                {
                    std::string success_cmd = "SECY ";
                    primary_res_msg.insert(primary_res_msg.begin(), success_cmd.begin(), success_cmd.end());
                }
                else
                {
                    std::string err_cmd = "SECN ";
                    primary_res_msg.insert(primary_res_msg.begin(), err_cmd.begin(), err_cmd.end());
                }
                // insert sequence number at the end of the command so the primary knows which operation it received an acknowledgement for
                std::vector<uint8_t> msg_seq_num = BeUtils::host_num_to_network_vector(next_operation.seq_num);
                primary_res_msg.insert(primary_res_msg.end(), msg_seq_num.begin(), msg_seq_num.end());
                // send response back to client (primary who initiated request)
                std::vector<uint8_t> res_seq_num = BeUtils::host_num_to_network_vector(next_operation.seq_num);
                send_response(primary_res_msg);

                // exit if no more operations left in holdback queue
                if (BackendServer::holdback_operations.size() == 0)
                {
                    break;
                }
                next_operation = BackendServer::holdback_operations.top();
            }
        }
        // operation was sent by a non-primary server - forward to primary
        else
        {
            // forward operation to primary and wait for primary's response
            kvs_client_logger.log("Received " + command + " - forwarding operation to primary", 20);
            std::vector<char> primary_res = forward_operation_to_primary(client_stream);
            // send response back to client that initiated request
            send_response(primary_res);
            return;
        }
    }
}

// ! check this logic below again about passing by ref vs value
// Note that inputs is NOT passed by reference because a copy of inputs is needed
// a copy is necessary because the inputs passed in will be used by the primary to perform the command after confirmation that all secondaries are done
// if we take a reference and modify it, it'll modify the data that primary will use to perform the command after
int KVSClient::send_operation_to_secondaries(std::vector<char> inputs)
{
    // Primary increments write sequence number on its server before sending message to all secondaries
    BackendServer::seq_num += 1;

    // place operation in your holdback queue to complete after all secondaries respond
    HoldbackOperation operation;
    operation.seq_num = BackendServer::seq_num;
    operation.msg = inputs;
    BackendServer::holdback_operations.push(operation);

    // convert seq number to vector and append to inputs
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(BackendServer::seq_num);
    inputs.insert(inputs.begin(), seq_num_vec.begin(), seq_num_vec.end());

    // insert command at the front of inputs
    std::string primary_cmd = "PWRT ";
    inputs.insert(inputs.begin(), primary_cmd.begin(), primary_cmd.end());

    // send operation to each secondary
    for (int secondary_port : BackendServer::secondary_ports)
    {
        // open connection to each secondary, store fd, and write operation to secondary
        BeUtils::open_connection(secondary_port, BackendServer::server_sock_fd);
        // ! perform error check for writes
        BeUtils::write(BackendServer::server_sock_fd, inputs);
    }

    return 0;

    // // wait for secondary to receive operation and send back confirmation that the operation was completed
    // kvs_client_logger.log("Waiting for confirmation from secondaries", 20);
    // // error occurred while waiting for secondary acknowledgement of operation
    // if (wait_for_secondary_acks(secondary_fds) < 0)
    // {
    //     kvs_client_logger.log("One or more secondaries did not send an ACK - command failed", 20);
    //     // ! how do we handle this situation? shouldn't we tell the secondaries to revert back?
    //     // ! or should we get all the acks, then respond with ok to actually finish the write?
    //     // ! maybe add another error message here
    //     return -1;
    // }

    // // all acks received
    // // ! read all of the file descriptors and ensure they were able to perform the put correctly
    // // ! if so, you can complete the write yourself

    // // iterate all secondary fds and close them
    // for (int secondary_fd : secondary_fds)
    // {
    //     close(secondary_fd);
    // }

    // kvs_client_logger.log("All servers acknowledged operation - completing operation on primary", 20);
    // return 0;
}

// // Poll secondaries and wait for acknowledgement from all secondaries (with timeout)
// // ! double check this logic
// int KVSClient::wait_for_secondary_acks(std::vector<int> &secondary_fds)
// {
//     // create poll vector containing all secondary fds
//     std::vector<pollfd> poll_secondary_fds(secondary_fds.size());
//     for (size_t i = 0; i < poll_secondary_fds.size(); i++)
//     {
//         poll_secondary_fds[i].fd = secondary_fds[i];
//         poll_secondary_fds[i].events = POLLIN;
//         poll_secondary_fds[i].revents = 0;
//     }

//     // wait a total of 2 seconds for secondaries to respond
//     int timeout_ms = 2000;
//     // track start time and current time
//     struct timespec start_time;
//     struct timespec current_time;
//     clock_gettime(CLOCK_MONOTONIC, &start_time);

//     // loop until time runs out or all secondaries have responded
//     size_t total_ready = 0;
//     while (true)
//     {
//         // Calculate elapsed time since start
//         clock_gettime(CLOCK_MONOTONIC, &current_time);
//         int elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 +
//                          (current_time.tv_nsec - start_time.tv_nsec) / 1000000;

//         // Poll to wait for events on secondary file descriptors with updated timeout
//         int num_ready = poll(poll_secondary_fds.data(), poll_secondary_fds.size(), timeout_ms - elapsed_ms);

//         // file descriptors ready, add the number that were ready for reading on this iteration
//         if (num_ready > 0)
//         {
//             // update total ready by the number ready to read on this iteration
//             total_ready += num_ready;
//             // check if all fds are ready to read from
//             if (total_ready == poll_secondary_fds.size())
//             {
//                 return 0;
//             }
//         }
//         // Either timeout occurred, or error occurred while polling - failure in both cases
//         else
//         {
//             return -1;
//         }
//     }
// }

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

void KVSClient::send_response(std::vector<char> &response_msg)
{
    BeUtils::write(client_fd, response_msg);
    kvs_client_logger.log("Response sent to client on port " + std::to_string(client_port), 20);
}

/**
 * READ-ONLY COMMANDS
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
 * WRITE COMMANDS
 */

std::vector<char> KVSClient::call_write_command(std::string command, std::vector<char> &inputs)
{
    // erase command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // call handler for command
    if (command == "putv")
    {
        return putv(inputs);
    }
    else if (command == "cput")
    {
        return cput(inputs);
    }
    else if (command == "delr")
    {
        return delr(inputs);
    }
    else if (command == "delv")
    {
        return delv(inputs);
    }
}

std::vector<char> KVSClient::putv(std::vector<char> &inputs)
{
    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to PUT(R,C,V) - row not found";
        kvs_client_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
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
        return res_bytes;
    }
    std::string col(row_end + 1, col_end);

    // remainder of input is value
    std::vector<char> val(col_end + 1, inputs.end());

    // log command and args
    kvs_client_logger.log("PUTV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->put_value(row, col, val);
    return response_msg;
}

std::vector<char> KVSClient::cput(std::vector<char> &inputs)
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
        return res_bytes;
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
        return res_bytes;
    }
    std::string col(row_end + 1, col_end);

    // clear inputs UP TO AND INCLUDING the last \b chara
    inputs.erase(inputs.begin(), col_end + 1);

    // remainder of input is value1 and value2

    // extract the number in front of val1
    uint32_t bytes_in_val1 = BeUtils::network_vector_to_host_num(inputs);

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
    return response_msg;
}

std::vector<char> KVSClient::delr(std::vector<char> &inputs)
{
    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // log command and args
    kvs_client_logger.log("DELR R[" + row + "]", 20);

    // retrieve tablet and delete row
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_row(row);
    return response_msg;
}

std::vector<char> KVSClient::delv(std::vector<char> &inputs)
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
        return res_bytes;
    }
    std::string row = delv_args.substr(0, col_index);
    std::string col = delv_args.substr(col_index + 1);

    // log command and args
    kvs_client_logger.log("DELV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_value(row, col);
    return response_msg;
}
