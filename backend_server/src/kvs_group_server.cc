#include "../include/kvs_group_server.h"

Logger kvs_group_server_logger("KVS Group Server");

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
    // set this thread's flag to false to indicate that thread should be joined
    BackendServer::group_server_connections[pthread_self()] = false;
    close(group_server_fd);
}

// @brief Parse client command and call corresponding handler
void KVSGroupServer::handle_command(std::vector<char> &byte_stream)
{
    // extract command from first 4 bytes and convert command to lowercase
    std::string command(byte_stream.begin(), byte_stream.begin() + 4);
    command = Utils::to_lowercase(command);

    // checkpointing and recovery commands
    if (command == "ckpt")
    {
        checkpoint(byte_stream);
        return;
    }
    else if (command == "done")
    {
        done(byte_stream);
        return;
    }
    else if (command == "reco")
    {
        assist_with_recovery(byte_stream);
        return;
    }

    // write commands

    // primary server
    if (BackendServer::is_primary)
    {
        // Reject writes if primary is currently checkpointing
        if (BackendServer::is_checkpointing)
        {
            // log and send error message
            send_error_response("Unable to accept write request - server currently checkpointing");
            return;
        }

        // write operation forwarded from a server
        if (command == "putv" || command == "cput" || command == "delr" || command == "delv" || command == "rnmr" || command == "rnmc")
        {
            execute_two_phase_commit(byte_stream);
        }
        else
        {
            // log and send error message
            send_error_response("Unrecognized command - this should NOT occur");
            return;
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
        std::string old_cp_file = BackendServer::disk_dir + tablet->range_start + "_" + tablet->range_end + "_tablet_v" + std::to_string(BackendServer::last_checkpoint);
        std::string new_cp_file = BackendServer::disk_dir + tablet->range_start + "_" + tablet->range_end + "_tablet_v" + std::to_string(version_num);
        std::string log_filename = BackendServer::disk_dir + tablet->log_filename;

        // if log file is empty, no need to serialize tablet. Update checkpoint file name and continue
        std::ifstream log_file;
        log_file.open(log_filename);
        bool log_is_empty = log_file.peek() == std::ifstream::traits_type::eof();
        log_file.close();
        if (log_is_empty && BackendServer::last_checkpoint != 0)
        {
            kvs_group_server_logger.log("CP[" + std::to_string(version_num) + "] No updates since last checkpoint for " + tablet->range_start + ":" + tablet->range_end + ". Skipping", 20);
            std::rename(old_cp_file.c_str(), new_cp_file.c_str());
        }
        // non-empty log file - updates were made since last checkpoint so tablet must be checkpointed
        else
        {
            // write new checkpoint file
            tablet->serialize(new_cp_file);
            // delete old checkpoint file
            std::remove(old_cp_file.c_str());
            kvs_group_server_logger.log("CP[" + std::to_string(version_num) + "] Checkpointed tablet " + tablet->range_start + ":" + tablet->range_end, 20);

            // clear tablet's log file
            std::ofstream log_file(log_filename, std::ofstream::trunc);
            log_file.close();
            kvs_group_server_logger.log("CP[" + std::to_string(version_num) + "] Cleared " + log_filename, 20);
        }
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
// RECOVERY HELP
// *********************************************

void KVSGroupServer::assist_with_recovery(std::vector<char> &inputs)
{
    kvs_group_server_logger.log("Primary server assisting with recovery", 20);

    // erase RECO command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);
    // extract checkpoint version number and erase from inputs
    uint32_t sent_checkpoint_version = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);
    // extract sequence number and erase from inputs
    uint32_t sent_seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // check if the requesting server requires your checkpoint files
    std::vector<char> response;
    bool checkpoint_required;
    // last check
    if (BackendServer::last_checkpoint != sent_checkpoint_version)
    {
        response.push_back('C');
        checkpoint_required = true;
    }
    else
    {
        response.push_back('N');
        checkpoint_required = false;
    }

    // iterate tablet range and add checkpoint file (if necessary) and log file
    for (std::string tablet_range : BackendServer::tablet_ranges)
    {
        // read entire log file into a vector and append to response
        std::string log_filename = BackendServer::disk_dir + tablet_range + "_log";
        std::vector<char> log_file_data = BeUtils::read_from_file_into_vec(log_filename);

        // read entire checkpoint file into a vector and append to response
        if (checkpoint_required)
        {
            std::string cp_filename = BackendServer::disk_dir + tablet_range + "_tablet_v" + std::to_string(BackendServer::last_checkpoint);
            std::vector<char> cp_file_data = BeUtils::read_from_file_into_vec(cp_filename);

            // append the size of the checkpoint file to the front of the vector
            std::vector<uint8_t> cp_file_size_vec = BeUtils::host_num_to_network_vector(cp_file_data.size());
            cp_file_data.insert(cp_file_data.begin(), cp_file_size_vec.begin(), cp_file_size_vec.end());

            // append the cp_file_data vector to the end of response
            response.insert(response.end(), cp_file_data.begin(), cp_file_data.end());

            // append the size of the log file file to the front of the vector
            std::vector<uint8_t> log_file_size_vec = BeUtils::host_num_to_network_vector(log_file_data.size());
            log_file_data.insert(log_file_data.begin(), log_file_size_vec.begin(), log_file_size_vec.end());

            // append the log_file_data vector to the end of response
            response.insert(response.end(), log_file_data.begin(), log_file_data.end());
        }
        // use sequence number to determine which portion of the log file the server needs
        else
        {
            // sequence number indicates that the server has an END log for that operation
            // they should receive any operations AFTER this sequence number

            // TODO go through the log and provide LOGS WITH SEQ NUM STRICTLY GREATER THAN SEQ NUM SENT BY SERVER

            // // append the size of the checkpoint file to the front of the vector
            // std::vector<uint8_t> log_file_size_vec = BeUtils::host_num_to_network_vector(log_file_data.size());
            // log_file_data.insert(log_file_data.begin(), log_file_size_vec.begin(), log_file_size_vec.end());

            // // append the log_file_data vector to the end of response
            // response.insert(response.end(), log_file_data.begin(), log_file_data.end());
        }
    }

    // send response back to server
    send_response(response);
}

// *********************************************
// 2PC PRIMARY COORDINATION METHODS
// *********************************************

/// @brief Coordinates 2PC for client that requested a write operation
void KVSGroupServer::execute_two_phase_commit(std::vector<char> &inputs)
{
    kvs_group_server_logger.log("Primary received write operation - executing 2PC", 20);

    // primary is centralized sequencer - acquire lock for sequence number and increment sequence number
    // This operation's seq number is saved for use during 2PC (since another primary thread may receive an operation and modify the sequence number)
    BackendServer::seq_num_lock.lock();
    BackendServer::seq_num += 1;
    uint32_t operation_seq_num = BackendServer::seq_num;
    BackendServer::seq_num_lock.unlock();

    // extract write command from inputs and convert to lowercase. Erase command from beginning of inputs
    std::string command(inputs.begin(), inputs.begin() + 4);
    command = Utils::to_lowercase(command);
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract row from inputs. Erase row from beginning of inputs (+1 to remove delimiter)
    auto row_end = std::find(inputs.begin(), inputs.end(), '\b');
    std::string row(inputs.begin(), row_end);
    inputs.erase(inputs.begin(), row_end + 1);

    // save the file name of the tablet log you'll be writing logs to
    std::string operation_log_filename = BackendServer::retrieve_data_tablet(row)->log_filename;

    // print operation and row
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Executing " + command + " on R[" + row + "]", 20);

    // open connection with secondary servers
    std::unordered_map<int, int> secondary_servers = BackendServer::open_connection_with_secondary_servers();
    // open connection with servers in recovery and add them to the map
    for (int port : BackendServer::ports_in_recovery)
    {
        int recovery_server_fd = BeUtils::open_connection(port);
        secondary_servers[port] = recovery_server_fd;
    }

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Opened connection with all secondaries", 20);

    // write BEGIN to log
    write_to_log(operation_log_filename, operation_seq_num, "BEGN");

    // Send PREPARE to all secondaries
    if (construct_and_send_prepare(operation_seq_num, command, row, secondary_servers) < 0)
    {
        // Failure while constructing and sending PREPARE
        clean_operation_state(secondary_servers);
        send_error_response("OP[" + std::to_string(operation_seq_num) + "] Unable to send PREPARE to secondary");
        return;
    }

    // Wait for votes from all secondaries (with timeout)
    bool all_secondaries_in_favor = handle_secondary_votes(operation_seq_num, secondary_servers);

    std::vector<char> response_msg;
    // send commit message if all secondaries voted yes
    if (all_secondaries_in_favor)
    {
        // write COMMIT to log - requires sequence number, command, row, inputs to commit transaction
        std::vector<char> commit_log = {'C', 'M', 'M', 'T'};
        commit_log.insert(commit_log.end(), command.begin(), command.end());                   // add command to log
        std::vector<uint8_t> row_size = BeUtils::host_num_to_network_vector(row.length());     // size of row
        commit_log.insert(commit_log.end(), row_size.begin(), row_size.end());                 // add row size to log
        commit_log.insert(commit_log.end(), row.begin(), row.end());                           // add row to log
        std::vector<uint8_t> inputs_size = BeUtils::host_num_to_network_vector(inputs.size()); // size of inputs
        commit_log.insert(commit_log.end(), inputs_size.begin(), inputs_size.end());           // add input size to log
        commit_log.insert(commit_log.end(), inputs.begin(), inputs.end());                     // add inputs to log
        write_to_log(operation_log_filename, operation_seq_num, commit_log);

        response_msg = construct_and_send_commit(operation_seq_num, command, row, inputs, secondary_servers);
    }
    // send abort message if all secondaries voted no
    else
    {
        // write ABORT to log - requires sequence number, command, row to abort transaction
        std::vector<char> abort_log = {'A', 'B', 'R', 'T'};
        std::vector<uint8_t> row_size = BeUtils::host_num_to_network_vector(row.length()); // size of row
        abort_log.insert(abort_log.end(), row_size.begin(), row_size.end());               // add row size to log
        abort_log.insert(abort_log.end(), row.begin(), row.end());                         // add row to log
        write_to_log(operation_log_filename, operation_seq_num, abort_log);

        response_msg = construct_and_send_abort(operation_seq_num, row, secondary_servers);
    }

    // wait for servers to respond with acks
    std::vector<int> dead_servers = BackendServer::wait_for_acks_from_servers(secondary_servers);
    // remove dead servers from map of servers so we're not waiting on an ACK from them
    for (int dead_server : dead_servers)
    {
        close(secondary_servers[dead_server]); // close fd for dead server
        secondary_servers.erase(dead_server);  // remove dead server from map
    }

    // read acks from all remaining servers, since these servers have read events available on their fds
    for (const auto &server : secondary_servers)
    {
        BeUtils::read_with_size(server.second); // we don't need to do anything with the ACKs
    }

    // write END to log
    write_to_log(operation_log_filename, operation_seq_num, "ENDT");

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Received ACKS from secondaries", 20);
    clean_operation_state(secondary_servers);
    send_response(response_msg);
}

/// @brief Constructs prepare command to send to secondary servers
// Example prepare command: PREP<SP>SEQ_#ROW (note there is no space between the sequence number and the row)
int KVSGroupServer::construct_and_send_prepare(uint32_t operation_seq_num, std::string &command, std::string &row, std::unordered_map<int, int> &secondary_servers)
{
    // Primary acquires exclusive lock on row
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    if (tablet->acquire_exclusive_row_lock(command, row) < 0)
    {
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Primary failed to acquire exclusive row lock for R[" + row + "]", 20);
        return -1;
    }
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Primary acquired exclusive row lock for R[" + row + "]", 20);

    // construct PREPARE message to send to secondaries
    std::vector<char> prepare_msg = {'P', 'R', 'E', 'P', ' '};
    prepare_msg.insert(prepare_msg.end(), command.begin(), command.end());                     // append command to PREPARE message
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num); // convert seq number to vector and append to prepare_msg
    prepare_msg.insert(prepare_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
    prepare_msg.insert(prepare_msg.end(), row.begin(), row.end()); // append row to prepare_msg

    // send prepare command to all secondaries
    BackendServer::send_message_to_servers(prepare_msg, secondary_servers);
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Primary sent PREPARE to all secondaries", 20);
    return 0;
}

/// @brief Handles votes from secondaries following PREPARE message
bool KVSGroupServer::handle_secondary_votes(uint32_t operation_seq_num, std::unordered_map<int, int> &secondary_servers)
{
    if (!secondary_servers.empty())
    {
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Waiting for votes from secondaries", 20);

        // construct a vector of fds to wait on with timeout
        std::vector<int> secondary_fds;
        for (const auto &server : secondary_servers)
        {
            secondary_fds.push_back(server.second);
        }

        // Wait up to 2 seconds for secondaries to respond to PREPARE command
        if (BeUtils::wait_for_events(secondary_fds, 2000) < 0)
        {
            kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Timeout exceeded - failed to receive votes from all secondaries", 20);
            return false;
        }

        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Received all votes from secondaries", 20);

        // read votes from all secondaries
        bool all_secondaries_in_favor = true;
        for (int secondary_fd : secondary_fds)
        {
            BeUtils::ReadResult secondary_read = BeUtils::read_with_size(secondary_fd);
            // return false if there was an error reading from a secondary
            if (secondary_read.error_code != 0)
            {
                kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Failed to read vote from secondary", 20);
                return false;
            }
            // process vote sent by secondary
            // extract vote from beginning of inputs
            std::string vote(secondary_read.byte_stream.begin(), secondary_read.byte_stream.begin() + 4);
            vote = Utils::to_lowercase(vote);
            if (vote == "secn")
            {
                all_secondaries_in_favor = false;
            }
        }

        return all_secondaries_in_favor;
    }
    // no secondaries to read from, send true
    return true;
}

/// @brief Construct and send COMMIT to secondary servers
std::vector<char> KVSGroupServer::construct_and_send_commit(uint32_t operation_seq_num, std::string &command, std::string &row, std::vector<char> &inputs, std::unordered_map<int, int> &secondary_servers)
{
    // all secondaries voted yes - construct commit message to send to all secondaries
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] All secondaries voted YES - sending COMMIT", 20);

    // execute write operation on primary
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Executing write operation on primary", 20);
    std::vector<char> response_msg = execute_write_operation(command, row, inputs);

    // construct commit message to send to all secondaries
    std::vector<char> commit_msg = {'C', 'M', 'M', 'T', ' '};
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num); // add sequence number to commit_msg
    commit_msg.insert(commit_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
    commit_msg.insert(commit_msg.end(), command.begin(), command.end()); // add command to commit_msg
    commit_msg.insert(commit_msg.end(), row.begin(), row.end());         // add row to commit_msg and add space after to differentiate remaining inputs
    commit_msg.push_back(' ');
    commit_msg.insert(commit_msg.end(), inputs.begin(), inputs.end()); // add remainder of inputs to commit_msg
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Sending CMMT message to secondaries", 20);

    // send abort/commit command
    BackendServer::send_message_to_servers(commit_msg, secondary_servers);
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Primary sent COMMIT to all secondaries", 20);
    return response_msg;
}

/// @brief Construct and send ABORT to secondary servers
std::vector<char> KVSGroupServer::construct_and_send_abort(uint32_t operation_seq_num, std::string &row, std::unordered_map<int, int> &secondary_servers)
{
    // at least one secondary voted no - construct abort message to send to all secondaries
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] At least one secondary voted NO - sending ABORT", 20);

    // release lock held by primary
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    tablet->release_exclusive_row_lock(row);

    // construct ABORT message
    std::vector<char> abort_msg = {'A', 'B', 'R', 'T', ' '};
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num); // append sequence number to message
    abort_msg.insert(abort_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
    abort_msg.insert(abort_msg.end(), row.begin(), row.end()); // append row to message

    // send abort/commit command
    BackendServer::send_message_to_servers(abort_msg, secondary_servers);
    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Primary sent ABORT to all secondaries", 20);

    // send response message back for aborted operation
    std::string response_msg = "-ER Operation aborted";
    return std::vector<char>(response_msg.begin(), response_msg.end());
}

// *********************************************
// 2PC SECONDARY RESPONSE METHODS
// *********************************************

/// @brief Secondary responds to prepare command
void KVSGroupServer::prepare(std::vector<char> &inputs)
{
    // erase PREP command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // save write operation
    std::string command(inputs.begin(), inputs.begin() + 4);
    command = Utils::to_lowercase(command);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // extract sequence number and erase from inputs
    uint32_t operation_seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // row is remainder of inputs
    std::string row(inputs.begin(), inputs.end());

    // retrieve tablet for requested row
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);

    // write BEGIN to log
    write_to_log(tablet->log_filename, operation_seq_num, "BEGN");

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary received PREPARE from primary", 20);

    // acquire an exclusive lock on the row
    std::vector<char> vote_response;
    // failed to acquire exclusive row lock
    if (!BackendServer::is_recovering && tablet->acquire_exclusive_row_lock(command, row) < 0)
    {
        // write ABORT to log - requires sequence number, command, row to abort transaction
        std::vector<char> abort_log = {'A', 'B', 'R', 'T'};
        std::vector<uint8_t> row_size = BeUtils::host_num_to_network_vector(row.length()); // size of row
        abort_log.insert(abort_log.end(), row_size.begin(), row_size.end());               // add row size to log
        abort_log.insert(abort_log.end(), row.begin(), row.end());                         // add row to log
        write_to_log(tablet->log_filename, operation_seq_num, abort_log);

        // construct vote
        vote_response = {'S', 'E', 'C', 'N', ' '};
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary voted SECN", 20);
    }
    // successfully acquired row lock
    else
    {
        // write PREPARE to log - requires sequence number, command and row to prepare transaction
        std::vector<char> prepare_log = {'P', 'R', 'E', 'P'};
        prepare_log.insert(prepare_log.end(), command.begin(), command.end());             // add command to log
        std::vector<uint8_t> row_size = BeUtils::host_num_to_network_vector(row.length()); // size of row
        prepare_log.insert(prepare_log.end(), row_size.begin(), row_size.end());           // add row size to log
        prepare_log.insert(prepare_log.end(), row.begin(), row.end());                     // add row to log
        write_to_log(tablet->log_filename, operation_seq_num, prepare_log);

        // construct vote
        vote_response = {'S', 'E', 'C', 'Y', ' '};
        kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary voted SECY", 20);
    }

    // convert seq number to vector and append to vote
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

    // extract command from inputs and convert to lowercase. Erase command from beginning of inputs
    std::string command(inputs.begin(), inputs.begin() + 4);
    command = Utils::to_lowercase(command);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // extract row from inputs. Erase row from beginning of inputs (+1 to remove delimiter)
    auto row_end = std::find(inputs.begin(), inputs.end(), ' ');
    std::string row(inputs.begin(), row_end);
    inputs.erase(inputs.begin(), row_end + 1);

    // write COMMIT to log - requires sequence number, command, row, inputs to commit transaction
    std::vector<char> commit_log = {'C', 'M', 'M', 'T'};
    commit_log.insert(commit_log.end(), command.begin(), command.end());                   // add command to log
    std::vector<uint8_t> row_size = BeUtils::host_num_to_network_vector(row.length());     // size of row
    commit_log.insert(commit_log.end(), row_size.begin(), row_size.end());                 // add row size to log
    commit_log.insert(commit_log.end(), row.begin(), row.end());                           // add row to log
    std::vector<uint8_t> inputs_size = BeUtils::host_num_to_network_vector(inputs.size()); // size of inputs
    commit_log.insert(commit_log.end(), inputs_size.begin(), inputs_size.end());           // add input size to log
    commit_log.insert(commit_log.end(), inputs.begin(), inputs.end());                     // add inputs to log
    std::string operation_log_filename = BackendServer::retrieve_data_tablet(row)->log_filename;
    write_to_log(operation_log_filename, operation_seq_num, commit_log);

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary received CMMT from primary for " + command + " on R[" + row + "]", 20);

    // execute write operation if server is not in recovery mode
    if (!BackendServer::is_recovering)
    {
        // execute write operation
        execute_write_operation(command, row, inputs);
    }

    // write END to log
    write_to_log(operation_log_filename, operation_seq_num, "ENDT");

    // update sequence number on this server now that END log has been written
    BackendServer::seq_num_lock.lock();
    BackendServer::seq_num = operation_seq_num;
    BackendServer::seq_num_lock.unlock();

    // send back ack
    std::vector<char> ack_response = {'A', 'C', 'K', 'N', ' '};
    // convert seq number to vector and append to ack
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

    kvs_group_server_logger.log("OP[" + std::to_string(operation_seq_num) + "] Secondary received ABORT from primary", 20);

    // row is remainder of inputs
    std::string row(inputs.begin(), inputs.end());

    // retrieve tablet for operation
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);

    // write ABORT to log - requires sequence number and row to abort transaction
    std::vector<char> abort_log = {'A', 'B', 'R', 'T'};
    std::vector<uint8_t> row_size = BeUtils::host_num_to_network_vector(row.length()); // size of row
    abort_log.insert(abort_log.end(), row_size.begin(), row_size.end());               // add row size to log
    abort_log.insert(abort_log.end(), row.begin(), row.end());                         // add row to log
    write_to_log(tablet->log_filename, operation_seq_num, abort_log);

    // release exclusive lock on row if server is not in recovery mode
    if (!BackendServer::is_recovering)
    {
        // release exclusive lock on row
        tablet->release_exclusive_row_lock(row);
    }

    // write END to log
    write_to_log(tablet->log_filename, operation_seq_num, "ENDT");

    // update sequence number on this server now that END log has been written
    BackendServer::seq_num_lock.lock();
    BackendServer::seq_num = operation_seq_num;
    BackendServer::seq_num_lock.unlock();

    // send ACK back to primary
    std::vector<char> ack_response = {'A', 'C', 'K', 'N', ' '};
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
    ack_response.insert(ack_response.end(), seq_num_vec.begin(), seq_num_vec.end());
    send_response(ack_response);
}

// *********************************************
// TABLET WRITE OPERATIONS
// *********************************************

// Need to send a copy of inputs here because secondary receives exact copy of this command, and inputs is modified heavily in write operations
std::vector<char> KVSGroupServer::execute_write_operation(std::string &command, std::string &row, std::vector<char> inputs)
{
    // call handler for command
    if (command == "putv")
    {
        return putv(row, inputs);
    }
    else if (command == "cput")
    {
        return cput(row, inputs);
    }
    else if (command == "delr")
    {
        return delr(row, inputs);
    }
    else if (command == "delv")
    {
        return delv(row, inputs);
    }
    else if (command == "rnmr")
    {
        return rnmr(row, inputs);
    }
    else if (command == "rnmc")
    {
        return rnmc(row, inputs);
    }
    kvs_group_server_logger.log("Unrecognized write command - should NOT occur", 40);
    std::vector<char> res;
    return res;
}

std::vector<char> KVSGroupServer::putv(std::string &row, std::vector<char> &inputs)
{
    // find index of \b to extract col from inputs
    auto col_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (col_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to PUT(R,C,V) - column not found";
        kvs_group_server_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string col(inputs.begin(), col_end);

    // remainder of input is value
    std::vector<char> val(col_end + 1, inputs.end());

    // log command and args
    kvs_group_server_logger.log("PUTV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and put value for row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->put_value(row, col, val);
    return response_msg;
}

std::vector<char> KVSGroupServer::cput(std::string &row, std::vector<char> &inputs)
{
    // find index of \b to extract col from inputs
    auto col_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (col_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to CPUT(R,C,V1,V2) - column not found";
        kvs_group_server_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string col(inputs.begin(), col_end);

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

std::vector<char> KVSGroupServer::delr(std::string &row, std::vector<char> &inputs)
{
    // log command and args
    kvs_group_server_logger.log("DELR R[" + row + "]", 20);

    // retrieve tablet and delete row
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_row(row);
    return response_msg;
}

std::vector<char> KVSGroupServer::delv(std::string &row, std::vector<char> &inputs)
{
    std::string col(inputs.begin(), inputs.end());

    // log command and args
    kvs_group_server_logger.log("DELV R[" + row + "] C[" + col + "]", 20);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->delete_value(row, col);
    return response_msg;
}

std::vector<char> KVSGroupServer::rnmr(std::string &row, std::vector<char> &inputs)
{
    // remainder of inputs is new row
    std::string new_row(inputs.begin(), inputs.end());

    // log command and args
    kvs_group_server_logger.log("RNMR R1[" + row + "] R2[" + new_row + "]", 20);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->rename_row(row, new_row);
    return response_msg;
}

std::vector<char> KVSGroupServer::rnmc(std::string &row, std::vector<char> &inputs)
{
    // find index of \b to extract old col from inputs
    auto col_end = std::find(inputs.begin(), inputs.end(), '\b');
    // \b not found in index
    if (col_end == inputs.end())
    {
        // log and send error message
        std::string err_msg = "-ER Malformed arguments to RNMC(R, C1, C2) - column not found";
        kvs_group_server_logger.log(err_msg, 40);
        std::vector<char> res_bytes(err_msg.begin(), err_msg.end());
        return res_bytes;
    }
    std::string old_col(inputs.begin(), col_end);
    // clear inputs UP TO AND INCLUDING the last \b char
    inputs.erase(inputs.begin(), col_end + 1);
    // remainder of inputs is new col
    std::string new_col(inputs.begin(), inputs.end());

    // log command and args
    kvs_group_server_logger.log("RNMC R[" + row + "] C1[" + old_col + "] C2[" + new_col + "]", 20);

    // retrieve tablet and delete value from row and col combination
    std::shared_ptr<Tablet> tablet = BackendServer::retrieve_data_tablet(row);
    std::vector<char> response_msg = tablet->rename_column(row, old_col, new_col);
    return response_msg;
}

// *********************************************
// CLIENT RESPONSE
// *********************************************

/// @brief constructs an error response and internally calls send_response() - should only be used for client communication
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
    BeUtils::write_with_size(group_server_fd, response_msg);
    kvs_group_server_logger.log("Response sent to connection on port " + std::to_string(group_server_port), 20);
}

// *********************************************
// 2PC STATE CLEANUP
// *********************************************

void KVSGroupServer::clean_operation_state(std::unordered_map<int, int> secondary_servers)
{
    // close all connections to secondary servers
    for (const auto &server : secondary_servers)
    {
        close(server.second);
    }
}

// *********************************************
// WRITE TO LOG
// *********************************************

int KVSGroupServer::write_to_log(std::string &log_filename, uint32_t operation_seq_num, const std::string &message)
{
    std::vector<char> message_vec(message.begin(), message.end());
    return write_to_log(log_filename, operation_seq_num, message_vec);
}

int KVSGroupServer::write_to_log(std::string &log_filename, uint32_t operation_seq_num, const std::vector<char> &message)
{
    // open log file in binary mode for writing
    std::ofstream log_file(BackendServer::disk_dir + log_filename, std::ofstream::out | std::ofstream::app | std::ofstream::binary);

    // verify log file was opened
    if (!log_file.is_open())
    {
        kvs_group_server_logger.log("Error opening log file", 40);
        log_file.close();
        return -1;
    }

    // write operation sequence number to file
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(operation_seq_num);
    log_file.write(reinterpret_cast<const char *>(seq_num_vec.data()), seq_num_vec.size());

    // write message to log file
    log_file.write(message.data(), message.size());
    log_file.close();

    kvs_group_server_logger.log("Wrote operation to tablet log file", 20);
    return 0;
}
