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

/**
 * GROUP COMMUNICATION METHODS - PRIMARY
 */

// @brief Coordinates 2PC for client that requested a write operation
void KVSGroupServer::execute_two_phase_commit(std::vector<char> &inputs)
{
    // Increments write sequence number on primary for operation
    // ! need to be careful about how seq num fits into failure scenarios (like if we can't find the row in the command)

    // acquire lock for sequence number, increment sequence number, and save this operation's seq_num
    BackendServer::seq_num_lock.lock();
    BackendServer::seq_num += 1;
    int operation_seq_num = BackendServer::seq_num;
    BackendServer::seq_num_lock.unlock();

    // place operation in holdback queue to complete after all secondaries respond
    // ! figure out if we need to do this first
    HoldbackOperation operation(operation_seq_num, inputs);
    BackendServer::holdback_operations.push(operation);

    // TODO you need to create an entry in the votes_recvd vector for this seq_num

    // ! what happens if we can't open a connection with all secondaries? Error check length of secondary_fds
    std::vector<int> secondary_fds = open_connection_with_secondary_fds();
    construct_and_send_prepare_cmd(operation_seq_num, inputs, secondary_fds);

    // once you send the message out to all of your secondaries, wait for a response for up to 2 seconds
    // ! how long should this duration be?
    if (BeUtils::wait_for_events(secondary_fds, 2000) < 0)
    {
        // ! timeout occurred while waiting, meaning some server responded or some failure occurred
    }

    // read from all secondary fds
    for (int secondary_fd : secondary_fds)
    {
        BeUtils::ReadResult msg_from_secondary = BeUtils::read(secondary_fd);
        if (msg_from_secondary.error_code != 0)
        {
            // ! error occurred while reading from secondary fds
        }
        // process vote sent by secondary
        handle_secondary_vote(msg_from_secondary.byte_stream);
    }

    // iterate votes_recvd and check that all votes were a SECY
    // ! might not need a lock for this since all votes for that seq num are done and no one is writing there anymore
    bool all_secondaries_in_favor = true;
    for (std::string &vote : BackendServer::votes_recvd[operation_seq_num])
    {
        if (vote == "secn")
        {
            all_secondaries_in_favor = false;
            break;
        }
    }

    // handle scenario where not all secondaries voted yes
    if (!all_secondaries_in_favor)
    {
        // send abort message to secondaries
        // this should cause them to release their locks
        // then you respond to the client from here
    }

    // all secondaries voted yes - perform the operation
    execute_write_operation();
}

// @brief Open connection with each secondary port and save fd for each connection
std::vector<int> KVSGroupServer::open_connection_with_secondary_fds()
{
    std::vector<int> secondary_fds;
    for (int secondary_port : BackendServer::secondary_ports)
    {
        // ! note that opening a connection might fail too, so maybe separate opening connection with writing
        int secondary_fd = BeUtils::open_connection(secondary_port);
        if (secondary_fd < 0)
            secondary_fds.push_back(secondary_fd);
    }
    return secondary_fds;
}

// @brief Constructs prepare command to send to secondary servers
// Example prepare command: PREP<SP>SEQ_#ROW (note there is no space between the sequence number and the row)
void KVSGroupServer::construct_and_send_prepare_cmd(int seq_num, std::vector<char> &inputs, std::vector<int> secondary_fds)
{
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
    std::vector<char> prepare_msg = {'P', 'R', 'E', 'P', ' '};
    // convert seq number to vector and append to prepare_msg
    std::vector<uint8_t> seq_num_vec = BeUtils::host_num_to_network_vector(seq_num);
    prepare_msg.insert(prepare_msg.end(), seq_num_vec.begin(), seq_num_vec.end());
    // append row to prepare_msg
    prepare_msg.insert(prepare_msg.end(), row.begin(), row.end());

    for (int secondary_fd : BackendServer::secondary_ports)
    {
        if (BeUtils::write(secondary_fd, prepare_msg) < 0)
        {
            // ! figure out how to properly handle this scenario where write to secondary fails
        }
    }
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
    kvs_group_server_logger.log("Received " + vote + " for operation " + std::to_string(operation_seq_num), 20);

    // acquire mutex to add vote for sequence number
    BackendServer::votes_recvd_lock.lock();
    BackendServer::votes_recvd[operation_seq_num].push_back(vote);
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
    kvs_group_server_logger.log("Received ackn for operation " + std::to_string(operation_seq_num), 20);

    // acquire mutex to add vote for sequence number
    BackendServer::acks_recvd_lock.lock();
    BackendServer::acks_recvd[operation_seq_num] += 1;
    // release mutex after vote has been added
    BackendServer::acks_recvd_lock.unlock();
}

/**
 * GROUP COMMUNICATION METHODS - SECONDARY
 */

// @brief Secondary responds to prepare command
void KVSGroupServer::prepare(std::vector<char> &inputs)
{
    // erase PREP command from beginning of inputs
    inputs.erase(inputs.begin(), inputs.begin() + 5);

    // extract sequence number and erase from inputs
    uint32_t seq_num = BeUtils::network_vector_to_host_num(inputs);
    inputs.erase(inputs.begin(), inputs.begin() + 4);

    // row is remainder of inputs
    std::vector<char> row = inputs;

    // wrap operation with its sequence number and add to secondary holdback queue
    HoldbackOperation operation(seq_num, row);
    BackendServer::holdback_operations.push(operation);

    // iterate holdback queue and figure out if the command at the front is the command you want to perform next (1 + seq_num)
    HoldbackOperation next_operation = BackendServer::holdback_operations.top();
    while (next_operation.seq_num == BackendServer::prep_seq_num + 1)
    {
        // pop this operation from secondary_holdback_operations
        BackendServer::holdback_operations.pop();
        // increment prepare sequence number on secondary
        BackendServer::prep_seq_num += 1;
        // construct row stored in holdback queue - this is the row we want to acquire a lock for
        std::string row(next_operation.msg.begin(), next_operation.msg.end());

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

/**
 * WRITE METHODS
 */

void KVSGroupServer::execute_write_operation(std::vector<char> &inputs)
{
}