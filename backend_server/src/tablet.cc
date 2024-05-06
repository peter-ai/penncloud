#include "../include/tablet.h"
#include "../utils/include/be_utils.h"

#include <fstream>

Logger tablet_logger("Tablet");

// *********************************************
// CONSTANTS
// *********************************************

const char Tablet::delimiter = '\b';
const std::string Tablet::ok = "+OK";
const std::string Tablet::err = "-ER";

// *********************************************
// READ OPERATIONS
// *********************************************

/// @brief Reads all columns at provided row
std::vector<char> Tablet::get_all_rows()
{
    row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks

    std::vector<char> response_msg;
    for (const auto &row : data)
    {
        row_locks.at(row.first).lock_shared();                                       // acquire shared lock on this row's mutex
        response_msg.insert(response_msg.end(), row.first.begin(), row.first.end()); // add row to response
        response_msg.push_back(delimiter);                                           // insert delimiter to separate rows
        row_locks.at(row.first).unlock_shared();                                     // release shared lock on row's mutex
    }
    // remove last added delimiter
    response_msg.pop_back();
    row_locks_mutex.unlock_shared(); // release shared lock on row_lock's mutex
    return response_msg;
}

/// @brief Reads all columns at provided row
std::vector<char> Tablet::get_row(std::string &row)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0)
    {
        tablet_logger.log("-ER Row not found", 20);
        return construct_msg("Row not found", true);
    }

    row_locks_mutex.lock_shared();   // acquire shared lock on row_locks to read mutex from row_locks
    row_locks.at(row).lock_shared(); // acquire shared lock on this row's mutex
    const auto &row_level_data = data.at(row);

    // iterate and store all columns
    std::vector<char> response_msg;
    for (const auto &col : row_level_data)
    {
        response_msg.insert(response_msg.end(), col.first.begin(), col.first.end());
        response_msg.push_back(delimiter); // insert delimiter to separate columns
    }
    // remove last added delimiter
    response_msg.pop_back();

    row_locks.at(row).unlock_shared(); // release shared lock on row's mutex
    row_locks_mutex.unlock_shared();   // release shared lock on row_lock's mutex

    // append +OK to response and send it back
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    response_msg.insert(response_msg.begin() + ok.size(), ' '); // Add a space after "+OK"

    return response_msg;
}

/// @brief Reads value at provided row and column
std::vector<char> Tablet::get_value(std::string &row, std::string &col)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0)
    {
        tablet_logger.log("-ER Row not found", 20);
        return construct_msg("Row not found", true);
    }

    row_locks_mutex.lock_shared();   // acquire shared lock on row_locks to read mutex from row_locks
    row_locks.at(row).lock_shared(); // acquire shared lock on this row's mutex
    const auto &row_level_data = data.at(row);

    // release shared lock and exit if col not found in row
    if (row_level_data.count(col) == 0)
    {
        row_locks.at(row).unlock_shared(); // release shared lock on row's mutex
        row_locks_mutex.unlock_shared();   // release shared lock on row_lock's mutex
        tablet_logger.log("-ER Column not found", 20);
        return construct_msg("Column not found", true);
    }

    // retrieve value from row
    std::vector<char> response_msg = row_level_data.at(col);

    row_locks.at(row).unlock_shared(); // release shared lock on row's mutex
    row_locks_mutex.unlock_shared();   // release shared lock on row_lock's mutex

    // append +OK to response and send it back
    tablet_logger.log("+OK Retrieved value at R[" + row + "], C[" + col + "]", 20);
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    response_msg.insert(response_msg.begin() + ok.size(), ' '); // Add a space after "+OK"
    return response_msg;
}

// *********************************************
// ACQUIRING/RELEASING LOCKS FOR WRITE
// *********************************************

/// @brief Acquires exclusive lock on row for a write operation
int Tablet::acquire_exclusive_row_lock(std::string &operation, std::string &row)
{
    // putv should first create the row if it doesn't exist
    if (operation == "putv" && data.count(row) == 0)
    {
        // acquire exclusive access to the row_locks map first to create a mutex for the new row
        row_locks_mutex.lock();
        row_locks[row];
        row_locks_mutex.unlock();

        row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks
        row_locks.at(row).lock();      // acquire exclusive lock on data map to create row
        // create entry for row in data map
        data.emplace(row, std::unordered_map<std::string, std::vector<char>>());
        tablet_logger.log("Created R[" + row + "]", 20);
        // return from here since locks have been acquired
        return 0;
    }

    // every other operation requires the row to already exist
    // Return error if row not found in map
    if (data.count(row) == 0)
    {
        tablet_logger.log("-ER Row not found", 20);
        // return construct_msg("Row not found", true);
        return -1;
    }

    // otherwise, acquire necessary write locks
    row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks
    row_locks.at(row).lock();      // acquire exclusive lock on data map to insert value
    return 0;
}

/// @brief Releases exclusive lock on row for a write operation
void Tablet::release_exclusive_row_lock(std::string &row)
{
    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // unlock shared lock on row locks
    tablet_logger.log("+OK Released exclusive row lock on R[" + row + "]", 20);
}

// *********************************************
// WRITE OPERATIONS
// *********************************************

/// @brief Puts value at provided row and column. Creates a row if necessary.
std::vector<char> Tablet::put_value(std::string &row, std::string &col, std::vector<char> &val)
{
    // get data at row
    auto &row_level_data = data.at(row);

    // add value at column
    row_level_data[col] = val;

    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // unlock shared lock on row locks

    tablet_logger.log("+OK Inserted value at R[" + row + "], C[" + col + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

/// @brief Conditionally put value at provided row and column if value at row + column matches curr_val
std::vector<char> Tablet::cond_put_value(std::string &row, std::string &col, std::vector<char> &curr_val, std::vector<char> &new_val)
{
    // get data at row
    auto &row_level_data = data.at(row);

    // exit if data at col does not match curr_val
    if (row_level_data[col] != curr_val)
    {
        row_locks.at(row).unlock();      // unlock exclusive lock on row
        row_locks_mutex.unlock_shared(); // release shared lock on row locks
        tablet_logger.log("-ER V1 provided does not match currently stored value", 20);
        return construct_msg("V1 provided does not match currently stored value", true);
    }

    // overwrite value at column with new value
    row_level_data[col] = new_val;

    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // release shared lock on row locks
    tablet_logger.log("+OK Conditionally inserted value at R[" + row + "], C[" + col + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

/// @brief Delete provided row
std::vector<char> Tablet::delete_row(std::string &row)
{
    // delete row
    data.erase(row);

    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // release shared lock on row locks

    // acquire exclusive access to the row_locks map to delete the mutex for the deleted row
    row_locks_mutex.lock();
    row_locks.erase(row);
    row_locks_mutex.unlock();

    tablet_logger.log("+OK Deleted R[" + row + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

/// @brief Delete value at provided row and column
std::vector<char> Tablet::delete_value(std::string &row, std::string &col)
{
    // get data at row
    auto &row_level_data = data.at(row);

    // delete value if row column exists
    if (row_level_data.count(col) == 0)
    {
        row_locks.at(row).unlock();      // unlock exclusive lock on row
        row_locks_mutex.unlock_shared(); // release shared lock on row locks
        tablet_logger.log("-ER Column not found", 20);
        return construct_msg("Column not found", true);
    }

    // delete value and associated column key
    row_level_data.erase(col);

    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // release shared lock on row locks

    tablet_logger.log("+OK Deleted value at R[" + row + "], C[" + col + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

/// @brief Rename row with name "old_row" to name "new_row"
std::vector<char> Tablet::rename_row(std::string &old_row, std::string &new_row)
{
    row_locks_mutex.unlock_shared(); // unlock shared lock on row_locks first to modify row_locks
    // acquire exclusive access on the row_locks map to create a mutex for the new row
    row_locks_mutex.lock();
    row_locks[new_row];
    row_locks_mutex.unlock();

    row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks
    row_locks.at(new_row).lock();  // acquire exclusive lock on data map to create new_row

    // copy data from old row into new row
    data[new_row] = data[old_row];
    // delete old row
    data.erase(old_row);

    row_locks.at(new_row).unlock();  // unlock exclusive lock on new_row
    row_locks.at(old_row).unlock();  // unlock exclusive lock on old_row
    row_locks_mutex.unlock_shared(); // release shared lock on row locks

    // acquire exclusive access to the row_locks map to delete the mutex for the old row
    row_locks_mutex.lock();
    row_locks.erase(old_row);
    row_locks_mutex.unlock();

    tablet_logger.log("+OK Renamed row R[" + old_row + "] to R[" + new_row + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

/// @brief Rename column with name "old_col" to name "new_col" in row
std::vector<char> Tablet::rename_column(std::string &row, std::string &old_col, std::string &new_col)
{
    // get data at row
    auto &row_level_data = data.at(row);

    // put data at old_col in new_col
    row_level_data[new_col] = row_level_data[old_col];
    // erase old_col key from row
    row_level_data.erase(old_col);

    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // unlock shared lock on row locks

    tablet_logger.log("+OK Renamed column at R[" + row + "] from C[" + old_col + "] to C[" + new_col + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

// *********************************************
// TABLET SERIALIZATION/DESERIALIZATION
// *********************************************

void Tablet::serialize(const std::string &file_name)
{
    // open file in binary mode for writing
    std::ofstream file(file_name, std::ofstream::out | std::ofstream::binary | std::ofstream::app);

    // verify file was opened
    if (!file.is_open())
    {
        tablet_logger.log("Error opening file for serialization", 40);
        file.close();
        return;
    }

    // write start and end range to file (each should be 1 character)
    file.write(range_start.c_str(), range_start.length());
    file.write(range_end.c_str(), range_end.length());

    row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks

    for (const auto &row : data)
    {
        row_locks.at(row.first).lock_shared(); // acquire shared lock on this row's mutex

        // write size of row name to file
        std::vector<uint8_t> row_name_size = BeUtils::host_num_to_network_vector(row.first.length());
        file.write(reinterpret_cast<const char *>(row_name_size.data()), row_name_size.size());
        // write row name to file
        file.write(row.first.c_str(), row.first.length());

        // get reference to data in current row
        const auto &row_level_column_map = data.at(row.first);

        // calculate size of column data for this row
        uint32_t column_data_size = 0;
        for (const auto &col : row_level_column_map)
        {
            // add size of column name, data it contains, and 8 bytes to store the size of each (4 for each)
            column_data_size = col.first.length() + col.second.size() + 8;
        }
        // write column data size to file
        std::vector<uint8_t> column_data_size_vec = BeUtils::host_num_to_network_vector(column_data_size);
        file.write(reinterpret_cast<const char *>(column_data_size_vec.data()), column_data_size_vec.size());

        // iterate columns and write each column and its data to the file
        for (const auto &col : row_level_column_map)
        {
            // write size of col name to file
            std::vector<uint8_t> col_name_size = BeUtils::host_num_to_network_vector(col.first.length());
            file.write(reinterpret_cast<const char *>(col_name_size.data()), col_name_size.size());
            // write col name to file
            file.write(col.first.c_str(), col.first.length());

            // write size of col data to file
            std::vector<uint8_t> col_data_size = BeUtils::host_num_to_network_vector(col.second.size());
            file.write(reinterpret_cast<const char *>(col_data_size.data()), col_data_size.size());
            // write col data to file
            file.write(col.second.data(), col.second.size());
        }

        row_locks.at(row.first).unlock_shared(); // release shared lock on row's mutex
    }

    row_locks_mutex.unlock_shared(); // release shared lock on row_lock's mutex
    file.close();
}

void Tablet::deserialize_from_file(const std::string &file_name)
{
    // [start_range][end_range][size of row key][row][size of chars representing map for rows col+val][size of col key][col][size of val][value]
    // 1. Basically, to deserialize, you would read the start range first, then the end range
    // 2. Then, read 4 characters to get the size of the row key. Then read that many characters to get the row.
    // 3. Then, read 4 characters to get the size of the inner map. Read that many characters from the map.
    // 4. Now you know to process in column/value in alternating fashion until you exhaust the bytes. Even an empty value will have a size value dedicated to it (would just store 0)
    // 5. Once you're done with an inner map, go back to step 2 and repeat.

    // for each row you construct, make sure you add an entry in row_locks for it so it has an associated mutex.

    // open file in binary mode for reading
    std::ifstream file;
    file.open(file_name, std::ifstream::in | std::ifstream::binary);

    // verify file was opened
    if (!file.is_open())
    {
        tablet_logger.log("Error opening file for deserialization", 40);
        file.close();
        return;
    }

    // read start and end range from file (each should be 1 character)
    std::vector<char> tablet_start(1);
    file.read(tablet_start.data(), 1);
    range_start = std::string(tablet_start.data(), tablet_start.size());

    std::vector<char> tablet_end(1);
    file.read(tablet_end.data(), 1);
    range_end = std::string(tablet_end.data(), tablet_end.size());

    // set log file name for this tablet
    log_filename = range_start + "_" + range_end + "_log";

    // read until the end of the file
    while (true)
    {
        // exit loop if we've reached the end of the file
        // must be located at the start in case the tablet is empty
        if (file.eof())
        {
            break;
        }

        // read 4 characters to get the size of the row key
        std::vector<char> row_name_size_vec(4);
        file.read(row_name_size_vec.data(), 4);
        uint32_t row_name_size = BeUtils::network_vector_to_host_num(row_name_size_vec);

        // extract the row name
        std::vector<char> row_name_vec(row_name_size);
        file.read(row_name_vec.data(), row_name_size);
        std::string row_name(row_name_vec.begin(), row_name_vec.end());

        // create mutex for row in row_locks
        row_locks[row_name];

        // create row in data map
        data[row_name];
        // get reference to row
        auto &row_level_column_map = data.at(row_name);

        // read 4 characters to get the size of all data for this row
        std::vector<char> row_data_size_vec(4);
        file.read(row_data_size_vec.data(), 4);
        uint32_t row_data_size = BeUtils::network_vector_to_host_num(row_data_size_vec);

        uint32_t row_data_processed = 0;
        while (row_data_processed < row_data_size)
        {
            // read 4 characters to get the size of the col key
            std::vector<char> col_name_size_vec(4);
            file.read(col_name_size_vec.data(), 4);
            uint32_t col_name_size = BeUtils::network_vector_to_host_num(col_name_size_vec);
            row_data_processed += 4;

            // extract the col name
            std::vector<char> col_name_vec(col_name_size);
            file.read(col_name_vec.data(), col_name_size);
            std::string col_name(col_name_vec.begin(), col_name_vec.end());
            row_data_processed += col_name_size;

            // read 4 characters to get the size of the col data
            std::vector<char> col_data_size_vec(4);
            file.read(col_data_size_vec.data(), 4);
            uint32_t col_data_size = BeUtils::network_vector_to_host_num(col_data_size_vec);
            row_data_processed += 4;

            // extract the col name
            std::vector<char> col_data(col_data_size);
            file.read(col_data.data(), col_data_size);
            row_data_processed += col_data_size;

            // add col_name and col_data to data map
            row_level_column_map[col_name] = col_data;
        }
    }

    file.close();
}

void Tablet::deserialize_from_stream(std::vector<char> &stream)
{
    // [start_range][end_range][size of row key][row][size of chars representing map for rows col+val][size of col key][col][size of val][value]
    // 1. Basically, to deserialize, you would read the start range first, then the end range
    // 2. Then, read 4 characters to get the size of the row key. Then read that many characters to get the row.
    // 3. Then, read 4 characters to get the size of the inner map. Read that many characters from the map.
    // 4. Now you know to process in column/value in alternating fashion until you exhaust the bytes. Even an empty value will have a size value dedicated to it (would just store 0)
    // 5. Once you're done with an inner map, go back to step 2 and repeat.

    // for each row you construct, make sure you add an entry in row_locks for it so it has an associated mutex.

    // read start and end range from file (each should be 1 character)
    range_start = std::string(stream.begin(), stream.begin() + 1);
    stream.erase(stream.begin());
    range_end = std::string(stream.begin(), stream.begin() + 1);
    stream.erase(stream.begin());

    // set log file name for this tablet
    log_filename = range_start + "_" + range_end + "_log";

    // read until the stream is empty
    while (!stream.empty())
    {
        // read 4 characters to get the size of the row key
        uint32_t row_name_size = BeUtils::network_vector_to_host_num(stream);
        stream.erase(stream.begin(), stream.begin() + 4);

        // extract the row name
        std::vector<char> row_name_vec(stream.begin(), stream.begin() + row_name_size);
        std::string row_name(row_name_vec.begin(), row_name_vec.end());
        stream.erase(stream.begin(), stream.begin() + row_name_size);

        // create mutex for row in row_locks
        row_locks[row_name];
        // create row in data map
        data[row_name];
        // get reference to row
        auto &row_level_column_map = data.at(row_name);

        // read 4 characters to get the size of all data for this row
        uint32_t row_data_size = BeUtils::network_vector_to_host_num(stream);
        stream.erase(stream.begin(), stream.begin() + 4);

        uint32_t row_data_processed = 0;
        while (row_data_processed < row_data_size)
        {
            // read 4 characters to get the size of the col key
            uint32_t col_name_size = BeUtils::network_vector_to_host_num(stream);
            stream.erase(stream.begin(), stream.begin() + 4);
            row_data_processed += 4;

            // extract the col name
            std::vector<char> col_name_vec(stream.begin(), stream.begin() + col_name_size);
            std::string col_name(col_name_vec.begin(), col_name_vec.end());
            stream.erase(stream.begin(), stream.begin() + col_name_size);
            row_data_processed += col_name_size;

            // read 4 characters to get the size of the col data
            uint32_t col_data_size = BeUtils::network_vector_to_host_num(stream);
            stream.erase(stream.begin(), stream.begin() + 4);
            row_data_processed += 4;

            // extract the col data
            std::vector<char> col_data(stream.begin(), stream.begin() + col_data_size);
            stream.erase(stream.begin(), stream.begin() + col_data_size);
            row_data_processed += col_data_size;

            // add col_name and col_data to data map
            row_level_column_map[col_name] = col_data;
        }
    }
}

// *********************************************
// CONSTRUCT RESPONSE
// *********************************************

// construct error/success message as vector of chars to send back to client given a string
std::vector<char> Tablet::construct_msg(const std::string &msg, bool error)
{
    std::vector<char> response_msg(msg.begin(), msg.end());
    if (error)
    {
        response_msg.insert(response_msg.begin(), err.begin(), err.end());
        response_msg.insert(response_msg.begin() + err.size(), ' '); // Add a space after "-ER"
    }
    else
    {
        response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
        response_msg.insert(response_msg.begin() + ok.size(), ' '); // Add a space after "+OK"
    }
    return response_msg;
}