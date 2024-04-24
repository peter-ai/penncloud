#include "../include/tablet.h"

/**
 * CONSTANTS
 */

const char Tablet::delimiter = '\b';
const std::string Tablet::ok = "+OK";
const std::string Tablet::err = "-ER";

/**
 *  READ-ONLY METHODS
 */

// @brief Reads all columns at provided row
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

    // append +OK to response and send it back
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    response_msg.insert(response_msg.begin() + ok.size(), ' '); // Add a space after "+OK"

    return response_msg;
}

// @brief Reads all columns at provided row
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

// @brief Reads value at provided row and column
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

/**
 *  WRITE METHODS
 */

// @brief Puts value at provided row and column. Creates a row if necessary.
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

// @brief Delete provided row
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

// @brief Delete value at provided row and column
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

// @brief Conditionally put value at provided row and column if value at row + column matches curr_val
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

/**
 * ACQUIRING/RELEASING LOCKS FOR WRITE METHODS
 */

// @brief Acquires exclusive lock on row for a write operation
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

// @brief Releases exclusive lock on row for a write operation
void Tablet::release_exclusive_row_lock(std::string &row)
{
    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // unlock shared lock on row locks
    tablet_logger.log("+OK Released exclusive row lock on R[" + row + "]", 20);
}

/**
 * SERIALIZATION/DESERIALIZATION METHODS
 */

void Tablet::serialize(const std::string &file_name)
{
    // Don't forget to start with the start range and end range
    // To serialize, you can take each row, build the vector of chars for it, and then append the size of the entire inner map to the start, then append to end of row size + row.
    // Repeat this until you get to the end
}

void Tablet::deserialize(const std::string &file_name)
{
    // [start_range][end_range][size of row key][row][size of chars representing map for rows col+val][size of col key][col][size of val][value]
    // 1. Basically, to deserialize, you would read the start range first, then the end range
    // 2. Then, read 4 characters to get the size of the row key. Then read that many characters to get the row.
    // 3. Then, read 4 characters to get the size of the inner map. Read that many characters from the map.
    // 4. Now you know to process in column/value in alternating fashion until you exhaust the bytes. Even an empty value will have a size value dedicated to it (would just store 0)
    // 5. Once you're done with an inner map, go back to step 2 and repeat.

    // for each row you construct, make sure you add an entry in row_locks for it so it has an associated mutex.
}

/**
 * RESPONSE CONSTRUCTION
 */

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