#include <poll.h>

#include "../include/kvs_group_server.h"
#include "../utils/include/be_utils.h"
#include "../include/backend_server.h"

// @brief Read data from group server and construct command to perform on KVS
void KVSGroupServer::read_from_group_server()
{
    std::vector<char> byte_stream;
    uint32_t bytes_left_in_command = 0;

    int bytes_recvd;
    while (true)
    {
        char buf[4096];
        bytes_recvd = recv(group_server_fd, buf, 4096, 0);
        if (bytes_recvd < 0)
        {
            kvs_group_server_logger.log("Error reading from group server", 40);
            break;
        }
        else if (bytes_recvd == 0)
        {
            kvs_group_server_logger.log("Group server closed connection", 20);
            break;
        }

        for (int i = 0; i < bytes_recvd; i++)
        {
            // command is not complete, append byte to byte_stream and decrement number of bytes left to read to complete command
            if (bytes_left_in_command != 0)
            {
                byte_stream.push_back(buf[i]);
                bytes_left_in_command--;

                // no bytes left in command - handle command and continue with additional bytes if sent
                if (bytes_left_in_command == 0)
                {
                    handle_command(byte_stream);
                    // clear byte_stream in preparation for the next command
                    byte_stream.clear();
                    // reset bytes left in comand to 0
                    bytes_left_in_command = 0;
                }
            }
            // command is complete, we need the next 4 bytes to determine how much bytes are in this command
            else
            {
                // now we have two situations
                // 1) byte_stream's size < 4, in which case the bytes left in the command is 0 only because the prev command was completed
                // 2) byte_stream's size >= 4
                if (byte_stream.size() < 4)
                {
                    byte_stream.push_back(buf[i]);
                    // parse size of command once byte_stream is 4 bytes long and store it in bytes_left_in_command
                    if (byte_stream.size() == 4)
                    {
                        // parse size of command from byte_stream
                        bytes_left_in_command = BeUtils::network_vector_to_host_num(byte_stream);
                        // clear byte_stream in preparation for the incoming data
                        byte_stream.clear();
                    }
                }
            }
        }
    }
    close(group_server_fd);
}

// @brief Parse client command and call corresponding handler
void KVSGroupServer::handle_command(std::vector<char> &byte_stream)
{
    // extract command from first 4 bytes and convert command to lowercase
    std::string command(byte_stream.begin(), byte_stream.begin() + 4);
    command = Utils::to_lowercase(command);

    // primary server
    if (BackendServer::is_primary)
    {
        // write operation forwarded from a server
        if (command == "putv" || command == "cput" || command == "delr" || command == "delv")
        {
            send_prepare(byte_stream);
        }
        else if (command == "secy" || command == "secn")
        {
            handle_secondary_vote(byte_stream);
        }
        else if (command == "ackn")
        {
            handle_secondary_acks(byte_stream);
        }
    }
    // secondary server
    else
    {
        if (command == "prep")
        {
            // erase first 5 values to remove command
            handle_prep(byte_stream);
        }
        else if (command == "cmmt")
        {
        }
        else if (command == "abrt")
        {
        }
    }
}

/**
 * GROUP COMMUNICATION METHODS - PRIMARY
 */

// @brief Primary sends PREP to all secondaries
// Example prepare command: PREP<SP>SEQ_#ROW (note there is no space between the sequence number and the row)
void KVSGroupServer::send_prepare(std::vector<char> &inputs)
{
    // Increments write sequence number on primary before sending message to all secondaries
    BackendServer::seq_num += 1;

    // place operation in holdback queue to complete after all secondaries respond
    HoldbackOperation operation(BackendServer::seq_num, inputs);
    BackendServer::holdback_operations.push(operation);

    // extract row from inputs (start at inputs.begin() + 5 to ignore command)
    auto row_end = std::find(inputs.begin() + 5, inputs.end(), '\b');
    // \b not found in index
    // ! figure out how to properly handle this scenario where row is not found
    if (row_end == inputs.end())
    {
        // // log and send error message
        // std::string err_msg = "-ER Malformed arguments - row not found in operation";
        // kvs_group_server_logger.log(err_msg, 40);
        // std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        // return res_bytes;
    }
    std::string row(inputs.begin(), row_end);

    // send prepare command to all secondaries
    std::vector<char> prepare_msg;
    std::string cmd = "PREP ";
    // insert command into msg
    prepare_msg.insert(prepare_msg.end(), cmd.begin(), cmd.end());
    // convert seq number to vector and append to prepare_msg
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(BackendServer::seq_num);
    prepare_msg.insert(prepare_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
    // append row to prepare_msg
    prepare_msg.insert(prepare_msg.end(), row.begin(), row.end());

    // send prepare_msg to each secondary
    for (int secondary_port : BackendServer::secondary_ports)
    {
        // open connection to each secondary, write prepare_msg to secondary, close connection
        int secondary_fd = BeUtils::open_connection(secondary_port);
        if (BeUtils::write(secondary_fd, prepare_msg) < 0)
        {
            // ! figure out how to properly handle this scenario where write to secondary fails
        }
        close(secondary_fd);
    }

    // once you send the message out to all of your secondaries, you need to wait until the sequence number map has all ports
}

// @brief Handles primary thread receiving vote from secondary
void KVSGroupServer::handle_secondary_vote(std::vector<char> &inputs)
{
    // extract vote from beginning of inputs
    std::string vote(inputs.begin(), inputs.begin() + 4);
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract sequence number and erase from inputs
    uint32_t seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    kvs_group_server_logger.log("Received " + vote + " for operation " + std::to_string(seq_num), 20);

    // acquire mutex to add vote for sequence number
    BackendServer::votes_recvd_mutex.lock();
    BackendServer::votes_recvd[seq_num].push_back(vote);

    // if the operation has as many votes as there are secondaries, notify the primary thread waiting on this condition
    if (BackendServer::votes_recvd[seq_num].size() == BackendServer::secondary_ports.size())
    {
        // send signal to primary thread waiting for this operation to complete
        BackendServer::votes_recvd_cv[seq_num].notify_one();
    }
    // release mutex after vote has been added
    BackendServer::votes_recvd_mutex.unlock();
}

// @brief Handles primary thread receives ACK from secondary
void KVSGroupServer::handle_secondary_acks(std::vector<char> &inputs)
{
    // erase ackn from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract sequence number
    uint32_t seq_num = BeUtils::network_vector_to_host_num(inputs);

    kvs_group_server_logger.log("Received ackn for operation " + std::to_string(seq_num), 20);

    // acquire mutex to increment number of acks for sequence number
    BackendServer::acks_recvd_mutex.lock();
    BackendServer::acks_recvd[seq_num] += 1;

    // if the operation has as many acks as there are secondaries, notify the primary thread waiting on this condition
    if (BackendServer::acks_recvd[seq_num] == BackendServer::secondary_ports.size())
    {
        // send signal to primary thread waiting for this operation to complete
        BackendServer::acks_recvd_cv[seq_num].notify_one();
    }
    // release mutex after vote has been added
    BackendServer::acks_recvd_mutex.unlock();
}

/**
 * GROUP COMMUNICATION METHODS - SECONDARY
 */

// // @brief Primary sends PREP to all secondaries
// // Example prepare command: PREP<SP>SEQ_#ROW (note there is no space between the sequence number and the row)
// void KVSGroupServer::handle_prep(std::vector<char> &inputs)
// {
//     // erase PREP command from beginning of inputs
//     inputs.erase(inputs.begin(), inputs.begin() + 5);

//     // extract sequence number and erase from inputs
//     uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);
//     inputs.erase(inputs.begin(), inputs.begin() + 4);

//     // row is remainder of inputs
//     std::vector<char> row = inputs;

//     // wrap operation with its sequence number and add to secondary holdback queue
//     HoldbackOperation operation(operation_seq_num, row);
//     BackendServer::holdback_operations.push(operation);

//     // iterate holdback queue and figure out if the command at the front is the command you want to perform next (1 + seq_num)
//     HoldbackOperation next_operation = BackendServer::holdback_operations.top();
//     while (next_operation.seq_num == BackendServer::seq_num + 1)
//     {
//         // pop this operation from secondary_holdback_operations
//         BackendServer::holdback_operations.pop();
//         // increment sequence number on secondary
//         BackendServer::seq_num += 1;
//         // extract command from first 4 bytes
//         std::string operation_cmd(next_operation.msg.begin(), next_operation.msg.begin() + 4);
//         // convert command to lowercase to standardize command
//         operation_cmd = Utils::to_lowercase(operation_cmd);
//         // perform write operation on secondary server
//         std::vector<char> response_msg = call_write_command(operation_cmd, next_operation.msg);

//         // ! if the operation was successful, send SECY, otherwise send SECN
//         std::vector<char> primary_res_msg;
//         if (response_msg.front() == '+')
//         {
//             std::string success_cmd = "SECY ";
//             primary_res_msg.insert(primary_res_msg.begin(), success_cmd.begin(), success_cmd.end());
//         }
//         else
//         {
//             std::string err_cmd = "SECN ";
//             primary_res_msg.insert(primary_res_msg.begin(), err_cmd.begin(), err_cmd.end());
//         }
//         // insert sequence number at the end of the command so the primary knows which operation it received an acknowledgement for
//         std::vector<uint8_t> msg_seq_num = BeUtils::host_num_to_network_vector(next_operation.seq_num);
//         primary_res_msg.insert(primary_res_msg.end(), msg_seq_num.begin(), msg_seq_num.end());
//         // send response back to client (primary who initiated request)
//         std::vector<uint8_t> res_seq_num = BeUtils::host_num_to_network_vector(next_operation.seq_num);
//         send_response(primary_res_msg);

//         // exit if no more operations left in holdback queue
//         if (BackendServer::holdback_operations.size() == 0)
//         {
//             break;
//         }
//         next_operation = BackendServer::holdback_operations.top();
//     }
// }
