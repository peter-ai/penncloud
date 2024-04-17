#include <iostream>

#include "../include/tablet.h"
#include "../../utils/include/utils.h"

Logger tablet_logger("Tablet");

// define constants
const char Tablet::delimiter = '\b';
const std::string Tablet::ok = "+OK";
const std::string Tablet::err = "-ER";

// read all columns from tablet data at supplied row
std::vector<char> Tablet::get_row(std::string &row)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0)
    {
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
        // insert delimiter to separate columns
        response_msg.push_back('\b');
    }
    // remove last added delimiter
    response_msg.pop_back();

    row_locks.at(row).unlock_shared(); // release shared lock on row's mutex
    row_locks_mutex.unlock_shared();   // release shared lock on row_lock's mutex

    // append +OK to response and send it back
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    return response_msg;
}

// read value at supplied row and column from tablet data
std::vector<char> Tablet::get_value(std::string &row, std::string &col)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0)
    {
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
        return construct_msg("Column not found", true);
    }

    // retrieve value from row
    std::vector<char> response_msg = row_level_data.at(col);

    row_locks.at(row).unlock_shared(); // release shared lock on row's mutex
    row_locks_mutex.unlock_shared();   // release shared lock on row_lock's mutex

    // append +OK to response and send it back
    response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    return response_msg;
}

// add value at supplied row and column to tablet data
std::vector<char> Tablet::put_value(std::string &row, std::string &col, std::vector<char> &val)
{
    // row must be created if it does not exist in the map
    if (data.count(row) == 0)
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
    }
    else
    {
        row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks
        row_locks.at(row).lock();      // acquire exclusive lock on data map to create row
    }

    // at this point, the row we want to update has an exclusive lock on it
    auto &row_level_data = data.at(row);

    // add value at column
    row_level_data[col] = val;

    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // unlock shared lock on row locks

    tablet_logger.log("Inserted value at R[" + row + "], C[" + col + "]", 20);
    std::vector<char> response_msg(ok.begin(), ok.end());
    return response_msg;
}

// delete row in tablet data
std::vector<char> Tablet::delete_row(std::string &row)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0)
    {
        tablet_logger.log("-ER Row not found", 20);
        return construct_msg("Row not found", true);
    }

    row_locks_mutex.lock_shared(); // acquire a shared lock on row_locks to read from row_locks
    row_locks.at(row).lock();      // acquire an exclusive lock on data map to delete row

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

// delete value at supplied row and column in tablet data
std::vector<char> Tablet::delete_value(std::string &row, std::string &col)
{
    // exit if row does not exist in map
    if (data.count(row) == 0)
    {
        return construct_msg("Row not found", true);
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();

    // acquire an exclusive lock on data map to delete value
    row_locks.at(row).lock();

    // at this point, the row we want to update has an exclusive lock on it
    auto &row_level_data = data.at(row);

    // delete value if row column exists
    if (row_level_data.count(col) != 0)
    {
        // delete value and associated column key
        row_level_data.erase(col);
    }

    // unlock exclusive lock on row
    row_locks.at(row).unlock();

    // release shared lock on row locks
    row_locks_mutex.unlock_shared();
}

// add value at supplied row and column to tablet data, ONLY if current value is val1
void Tablet::cond_put_value(std::string &row, std::string &col, std::vector<char> &old_val, std::vector<char> &new_val)
{
    // get the value at the row and col
    // this ensures we're not exclusively locking unless we're sure we need to write
    const std::vector<char> &retrieved_val = get_value(row, col);

    // exit if retrieved_val does not match old_val
    if (retrieved_val.size() != old_val.size() || retrieved_val != old_val)
    {
        return;
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();

    // acquire exclusive access to row to conditionally update value
    row_locks.at(row).lock();
    auto &row_level_data = data.at(row);

    // overwrite value at column
    row_level_data[col] = new_val;

    // unlock exclusive lock on row
    row_locks.at(row).unlock();

    // release shared lock on row locks
    row_locks_mutex.unlock_shared();
}

// construct error/success message as vector of chars to send back to client given a string
std::vector<char> Tablet::construct_msg(const std::string &msg, bool error)
{
    std::vector<char> response_msg(msg.begin(), msg.end());
    if (error)
    {
        response_msg.insert(response_msg.begin(), err.begin(), err.end());
    }
    else
    {
        response_msg.insert(response_msg.begin(), ok.begin(), ok.end());
    }
    return response_msg;
}

// // construct error/success message as vector of chars to send back to client given a vector of chars
// std::vector<char> Tablet::construct_msg(std::vector<char>& msg, bool error) {
//     if (error) {
//         msg.insert(msg.begin(), err.begin(), err.end());
//     } else {
//         msg.insert(msg.begin(), ok.begin(), ok.end());
//     }
//     return msg;
// }
