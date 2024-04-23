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
    }
    // secondary server
    else
    {
    }
}

/**
 * GROUP COMMUNICATION METHODS
 */

// @brief Primary sends PREP to all secondaries
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
}
