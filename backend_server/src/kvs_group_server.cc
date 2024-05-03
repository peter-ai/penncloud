#include <poll.h>

#include "../../utils/include/utils.h"
#include "../include/kvs_group_server.h"
#include "../utils/include/be_utils.h"
#include "../include/backend_server.h"

// *********************************************
// MAIN RUN METHODS
// *********************************************

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

    // checkpointing commands
    if (command == "ckpt")
    {
        checkpoint(byte_stream);
    }
    else if (command == "done")
    {
        done(byte_stream);
    }
    // write commands
    else
    {
        // primary server
        if (BackendServer::is_primary)
        {
            // write operation forwarded from a server
            if (command == "putv" || command == "cput" || command == "delr" || command == "delv")
            {
                execute_two_phase_commit(byte_stream);
            }
        }
        // secondary server
        else
        {
            if (command == "prep")
            {
                prepare(byte_stream);
            }
            else if (command == "cmmt")
            {
                commit(byte_stream);
            }
            else if (command == "abrt")
            {
                abort(byte_stream);
            }
        }
    }
}

// *********************************************
// CHECKPOINTING METHODS
// *********************************************

void KVSGroupServer::checkpoint(std::vector<char> &inputs)
{
    // erase CKPT command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract checkpoint version number and erase from inputs
    uint32_t version_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    kvs_group_server_logger.log("CP[" + std::to_string(version_num) + "] Server beginning checkpointing", 20);

    // Checkpoint all tablets on server
    for (const auto &tablet : BackendServer::server_tablets)
    {
        // file name of tablet - start_end_tablet_v# (# is operation_seq_num)
        std::string old_cp_file = tablet->range_start + "_" + tablet->range_end + "_tablet_v" + std::to_string(BackendServer::last_checkpoint);
        std::string new_cp_file = tablet->range_start + "_" + tablet->range_end + "_tablet_v" + std::to_string(version_num);
        // write new checkpoint file
        tablet->serialize(new_cp_file);
        // delete old checkpoint file
        std::remove(old_cp_file.c_str());
        kvs_group_server_logger.log("CP[" + std::to_string(version_num) + "] Checkpointed tablet " + tablet->range_start + ":" + tablet->range_end, 20);
    }

    // update version number of last checkpoint on this server
    BackendServer::last_checkpoint = version_num;

    // Send acknowledgement back to primary
    std::vector<char> ack_response = {'A', 'C', 'K', 'N', ' '};
    // convert seq number to vector and append to prepare_msg
    std::vector<uint8_t> version_num_vec = BeUtils::host_num_to_network_vector(version_num);
    ack_response.insert(ack_response.end(), version_num_vec.begin(), version_num_vec.end());
    send_response(ack_response);
}

void KVSGroupServer::done(std::vector<char> &inputs)
{
    // Since we're not queueing outgoing messages and simply retrying from the frontend, this does not do anything
    // erase DONE command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract checkpoint version number and erase from inputs
    uint32_t version_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    kvs_group_server_logger.log("CP[" + std::to_string(version_num) + "] Checkpoint complete - server received DONE", 20);
}

// *********************************************
// 2PC PRIMARY COORDINATION METHODS
// *********************************************

// @brief Coordinates 2PC for client that requested a write operation
void KVSGroupServer::execute_two_phase_commit(std::vector<char> &inputs)
{
    kvs_group_server_logger.log("Primary received write operation - executing two-phase commit", 20);

    // acquire lock for sequence number, increment sequence number, and save this operation's seq_num
    BackendServer::seq_num_lock.lock();
    BackendServer::seq_num += 1;
    int operation_seq_num = BackendServer::seq_num;
    BackendServer::seq_num_lock.unlock();

    // create entry in votes_rcvd and acks_rcvd map
    BackendServer::votes_recvd[operation_seq_num];
    BackendServer::acks_recvd[operation_seq_num];

    // open connection with secondary servers
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Opening connection with all secondaries", 20);
    std::vector<int> secondary_fds = open_connection_with_secondary_fds();
    // Failed to establish a connection with all secondary servers
    if (secondary_fds.size() != BackendServer::secondary_ports.size())
    {
        clean_operation_state(operation_seq_num, secondary_fds);
        send_error_response("OP[" + std::to_string(operation_seq_num) + "] Unable to establish connection with all secondaries");
        return;
    }
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Successfully opened connection with all secondaries", 20);

    // Send prepare command to all secondaries.
    if (construct_and_send_prepare_cmd(operation_seq_num, inputs, secondary_fds) < 0)
    {
        // Failure while constructing and sending prepare command
        clean_operation_state(operation_seq_num, secondary_fds);
        send_error_response("OP[" + std::to_string(operation_seq_num) + "] Unable to send PREP command to secondary");
        return;
    }

    // Wait 2 seconds for secondaries to response to prepare command
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Waiting for votes from secondaries", 20);
    if (BeUtils::wait_for_events(secondary_fds, 2000) < 0)
    {
        clean_operation_state(operation_seq_num, secondary_fds);
        send_error_response("OP[" + std::to_string(operation_seq_num) + "] Failed to receive votes from all secondaries");
        return;
    }
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Received all votes from secondaries", 20);

    // read votes from all secondaries
    for (int secondary_fd : secondary_fds)
    {
        BeUtils::ReadResult msg_from_secondary = BeUtils::read(secondary_fd);
        if (msg_from_secondary.error_code != 0)
        {
            clean_operation_state(operation_seq_num, secondary_fds);
            send_error_response("OP[" + std::to_string(operation_seq_num) + "] Failed to read vote from secondary");
            return;
        }
        // process vote sent by secondary
        handle_secondary_vote(msg_from_secondary.byte_stream);
    }

    // iterate votes_recvd and check that all votes were a SECY
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Checking votes from secondaries", 20);
    bool all_secondaries_in_favor = true;
    for (std::string &vote : BackendServer::votes_recvd.at(operation_seq_num))
    {
        if (vote == "secn")
        {
            all_secondaries_in_favor = false;
            break;
        }
    }

    std::vector<char> response_msg;
    std::vector<char> abort_commit_msg;
    if (all_secondaries_in_favor)
    {
        // all secondaries voted yes
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] All secondaries voted yes", 20);

        // execute write operation on primary
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Executing write operation on primary", 20);
        response_msg = execute_write_operation(inputs);

        // construct commit message to send to all secondaries
        abort_commit_msg = {'C', 'M', 'M', 'T', ' '};
        // convert seq number to vector and append to prepare_msg
        std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
        abort_commit_msg.insert(abort_commit_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
        // append inputs to commit_msg
        abort_commit_msg.insert(abort_commit_msg.end(), inputs.begin(), inputs.end());
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Sending CMMT message to secondaries", 20);
    }
    else
    {
        // at least one secondary voted no - construct abort message to send to all secondaries
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] At least one secondary voted no", 20);
        // extract row from inptus
        std::string row = extract_row_from_input(inputs);

        // release lock held by primary
        std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
        tablet->release_exclusive_row_lock(row);

        abort_commit_msg = {'A', 'B', 'R', 'T', ' '};
        // convert seq number to vector and append to prepare_msg
        std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
        abort_commit_msg.insert(abort_commit_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
        abort_commit_msg.insert(abort_commit_msg.end(), row.begin(), row.end());
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Sending ABRT message to secondaries", 20);
    }

    // send abort/commit command
    for (int secondary_fd : secondary_fds)
    {
        // exit if write failure occurs
        if (BeUtils::write(secondary_fd, abort_commit_msg) < 0)
        {
            // Failure while constructing and sending prepare command
            clean_operation_state(operation_seq_num, secondary_fds);
            send_error_response("OP[" + std::to_string(operation_seq_num) + "] Unable to send CMMT/ABRT command to secondary");
            return;
        }
    }
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Successfully sent CMMT/ABRT command to all secondaries", 20);

    // Wait 2 seconds for secondaries to response to prepare command
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Waiting for ACKS from secondaries", 20);
    if (BeUtils::wait_for_events(secondary_fds, 2000) < 0)
    {
        clean_operation_state(operation_seq_num, secondary_fds);
        send_error_response("OP[" + std::to_string(operation_seq_num) + "] Failed to receive ACK from all secondaries");
        return;
    }
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Received all ACKS from secondaries", 20);

    // read acks from all secondaries
    for (int secondary_fd : secondary_fds)
    {
        BeUtils::ReadResult msg_from_secondary = BeUtils::read(secondary_fd);
        if (msg_from_secondary.error_code != 0)
        {
            clean_operation_state(operation_seq_num, secondary_fds);
            send_error_response("OP[" + std::to_string(operation_seq_num) + "] Failed to read ACK from secondary");
            return;
        }
        // process ack sent by secondary
        handle_secondary_ack(msg_from_secondary.byte_stream);
    }

    // didn't receive all acks (shouldn't occur)
    if (BackendServer::acks_recvd.at(operation_seq_num) != BackendServer::secondary_ports.size())
    {
        clean_operation_state(operation_seq_num, secondary_fds);
        send_error_response("OP[" + std::to_string(operation_seq_num) + "] Failed to receive all ACKS from secondaries");
        return;
    }

    clean_operation_state(operation_seq_num, secondary_fds);
    send_response(response_msg);
}

std::string KVSGroupServer::extract_row_from_input(std::vector<char> &inputs)
{
    // extract row from inputs (start at inputs.begin() + 5 to ignore command)
    auto row_end = std::find(inputs.begin() + 5, inputs.end(), '\b');
    std::string row(inputs.begin() + 5, row_end);
    return row;
}

// @brief Constructs prepare command to send to secondary servers
// Example prepare command: PREP<SP>SEQ_#ROW (note there is no space between the sequence number and the row)
int KVSGroupServer::construct_and_send_prepare_cmd(int operation_seq_num, std::vector<char> &inputs, std::vector<int> secondary_fds)
{
    std::string write_operation(inputs.begin(), inputs.begin() + 4); // extract requested write operation from inputs
    write_operation = Utils::to_lowercase(write_operation);

    // extract row from inputs (start at inputs.begin() + 5 to ignore command)
    auto row_end = std::find(inputs.begin() + 5, inputs.end(), '\b');
    // Unable to extract row from inputs due to improper delimiter
    if (row_end == inputs.end())
    {
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Malformed row argument while constructing PREP command", 20);
        return -1;
    }
    std::string row(inputs.begin() + 5, row_end);

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Acquiring exclusive row lock on primary for R[" + row + "]", 20);

    // Primary acquires exclusive lock on row
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    if (tablet->acquire_exclusive_row_lock(write_operation, row) < 0)
    {
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Primary failed to acquire exclusive row lock", 20);
        return -1;
    }

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Sending PREP command to all secondaries", 20);

    // construct PREP command to send to secondaries
    std::vector<char> prepare_msg = {'P', 'R', 'E', 'P', ' '};
    prepare_msg.insert(prepare_msg.end(), write_operation.begin(), write_operation.end());     // append write operation to prepare_msg
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num); // convert seq number to vector and append to prepare_msg
    prepare_msg.insert(prepare_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
    prepare_msg.insert(prepare_msg.end(), row.begin(), row.end()); // append row to prepare_msg

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] PREP COMMAND - " + std::string(prepare_msg.begin(), prepare_msg.end()), 20);

    // send prepare command to all secondary
    for (int secondary_fd : secondary_fds)
    {
        // exit if write failure occurs
        if (BeUtils::write(secondary_fd, prepare_msg) < 0)
        {
            kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Failure while writing PREP command to secondary", 20);
            return -1;
        }
    }
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Successfully sent PREP command to all secondaries", 20);
    return 0;
}

// @brief Handles primary thread receiving vote from secondary
void KVSGroupServer::handle_secondary_vote(std::vector<char> &inputs)
{
    // extract vote from beginning of inputs
    std::string vote(inputs.begin(), inputs.begin() + 4);
    inputs.erase(inputs.begin(), inputs.begin() + 5);
    // extract sequence number
    uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);

    // log vote
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Received " + vote + " from secondary", 20);

    // acquire mutex to add vote for sequence number
    BackendServer::votes_recvd_lock.lock();
    BackendServer::votes_recvd.at(operation_seq_num).push_back(vote);
    // release mutex after vote has been added
    BackendServer::votes_recvd_lock.unlock();
}

// @brief Handles primary thread receiving ACKN from secondary
void KVSGroupServer::handle_secondary_ack(std::vector<char> &inputs)
{
    // erase ackn from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);
    // extract sequence number
    uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);

    // log acknowledgement
    kvs_group_server_logger.log("Received ACK for operation " + std::to_string(operation_seq_num), 20);

    // acquire mutex to add vote for sequence number
    BackendServer::acks_recvd_lock.lock();
    BackendServer::acks_recvd.at(operation_seq_num) += 1;
    // release mutex after vote has been added
    BackendServer::acks_recvd_lock.unlock();
}

// *********************************************
// 2PC SECONDARY RESPONSE METHODS
// *********************************************

// @brief Secondary responds to prepare command
void KVSGroupServer::prepare(std::vector<char> &inputs)
{
    // erase PREP command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // save write operation
    std::string write_operation(inputs.begin(), inputs.begin() + 4);
    write_operation = Utils::to_lowercase(write_operation);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // extract sequence number and erase from inputs
    uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // row is remainder of inputs
    std::string row(inputs.begin(), inputs.end());

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary received PREP from primary", 20);

    // acquire an exclusive lock on the row
    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> vote_response;
    // failed to acquire exclusive row lock
    if (tablet->acquire_exclusive_row_lock(write_operation, row) < 0)
    {
        vote_response = {'S', 'E', 'C', 'N', ' '};
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary responded to prepare with SECY", 20);
    }
    // successfully acquired row lock
    else
    {
        vote_response = {'S', 'E', 'C', 'Y', ' '};
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary responded to prepare with SECN", 20);
    }

    // convert seq number to vector and append to prepare_msg
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
    vote_response.insert(vote_response.end(), seq_num_vec.begin(), seq_num_vec.end());
    send_response(vote_response);
}

// @brief Secondary responds to commit command
void KVSGroupServer::commit(std::vector<char> &inputs)
{
    // erase CMMT command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract sequence number and erase from inputs
    uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary received CMMT from primary", 20);

    // remainder of command is passed to execute write
    // execute write operation on primary
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Executing write operation on secondary", 20);
    std::vector<char> response_msg = execute_write_operation(inputs);

    // ! what happens if the write fails on the secondary? It sends ACK either way

    std::vector<char> ack_response = {'A', 'C', 'K', 'N', ' '};
    // convert seq number to vector and append to prepare_msg
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
    ack_response.insert(ack_response.end(), seq_num_vec.begin(), seq_num_vec.end());
    send_response(ack_response);
}

// @brief Secondary responds to abort command
void KVSGroupServer::abort(std::vector<char> &inputs)
{
    // erase ABRT command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract sequence number and erase from inputs
    uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary received ABRT from primary", 20);

    // row is remainder of inputs
    std::string row(inputs.begin(), inputs.end());

    // acquire an exclusive lock on the row
    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    tablet->release_exclusive_row_lock(row);

    std::vector<char> ack_response = {'A', 'C', 'K', 'N', ' '};
    // convert seq number to vector and append to prepare_msg
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
    ack_response.insert(ack_response.end(), seq_num_vec.begin(), seq_num_vec.end());
    send_response(ack_response);
}

// *********************************************
// TABLET WRITE OPERATIONS
// *********************************************

// Need to send a copy of inputs here because secondary receives exact copy of this command, and inputs is modified heavily in write operations
std::vector<char> KVSGroupServer::execute_write_operation(std::vector<char> inputs)
{
    // extract command from beginning of inputs and erase command
    std::string command(inputs.begin(), inputs.begin() + 4);
    command = Utils::to_lowercase(command);
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
    kvs_group_server_logger.log("Unrecognized write command - should NOT occur", 40);
    std::vector<char> res;
    return res;
}

std::vector<char> KVSGroupServer::putv(std::vector<char> &inputs)
{
    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to PUT(R,C,V) - row not found";
        kvs_group_server_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string row(inputs.begin(), row_end);

    // find index of \b to extract col from inputs
    auto col_end = std::find(row_end + 1, inputs.end(), '\b');
    // \b not found in index
    if (col_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to PUT(R,C,V) - column not found";
        kvs_group_server_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string col(row_end + 1, col_end);

    // remainder of input is value
    std::vector<char> val(col_end + 1, inputs.end());

    // log command and args
    kvs_group_server_logger.log("PUTV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->put_value(row, col, val);
    return response_msg;
}

std::vector<char> KVSGroupServer::cput(std::vector<char> &inputs)
{
    // find index of \b to extract row from inputs
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (row_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to CPUT(R,C,V1,V2) - row not found";
        kvs_group_server_logger.log(err_msg, 40);
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
        kvs_group_server_logger.log(err_msg, 40);
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
    kvs_group_server_logger.log("CPUT R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and call CPUT on tablet
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->cond_put_value(row, col, val1, val2);
    return response_msg;
}

std::vector<char> KVSGroupServer::delr(std::vector<char> &inputs)
{
    // extract row as string from inputs
    std::string row(inputs.begin(), inputs.end());
    // log command and args
    kvs_group_server_logger.log("DELR R[" + row + "]", 20);

    // retrieve tablet and delete row
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_row(row);
    return response_msg;
}

std::vector<char> KVSGroupServer::delv(std::vector<char> &inputs)
{
    // convert vector to string since row and column are string-compatible values and split on delimiter
    std::string delv_args(inputs.begin(), inputs.end());

    size_t col_index = delv_args.find_first_of('\b');
    // delimiter not found in string - should be present to split row and column
    if (col_index == std::string::npos)
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to DELV(R,C) - delimiter after row not found";
        kvs_group_server_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string row = delv_args.substr(0, col_index);
    std::string col = delv_args.substr(col_index + 1);

    // log command and args
    kvs_group_server_logger.log("DELV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_value(row, col);
    return response_msg;
}

/**
 * SENDING CLIENT RESPONSE
 */

// @brief constructs an error response and internally calls send_response()
void KVSGroupServer::send_error_response(const std::string &err_msg)
{
    // add "-ER " to front of error message
    std::string prepared_err_msg = "-ER " + err_msg;
    // log message
    kvs_group_server_logger.log(prepared_err_msg, 40);
    // construct vector of chars to send reponse
    std::vector<char> res_bytes(prepared_err_msg.begin(), prepared_err_msg.end());
    send_response(res_bytes);
}

void KVSGroupServer::send_response(std::vector<char> &response_msg)
{
    BeUtils::write(group_server_fd, response_msg);
    kvs_group_server_logger.log("Response sent to client on port " + std::to_string(group_server_port), 20);
}

// *********************************************
// 2PC STATE CLEANUP
// *********************************************

void KVSGroupServer::clean_operation_state(int operation_seq_num, std::vector<int> secondary_fds)
{
    // remove operation from votes map
    BackendServer::votes_recvd_lock.lock();
    BackendServer::votes_recvd.erase(operation_seq_num);
    BackendServer::votes_recvd_lock.unlock();

    // remove operation from acks map
    BackendServer::acks_recvd_lock.lock();
    BackendServer::acks_recvd.erase(operation_seq_num);
    BackendServer::acks_recvd_lock.unlock();

    // close all connections to secondary servers
    for (int secondary_fd : secondary_fds)
    {
        close(secondary_fd);
    }
}
