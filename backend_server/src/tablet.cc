#include "../include/tablet.h"

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

// read value at supplied row and column from tablet data
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

void Tablet::release_exclusive_row_lock(std::string &row)
{
    row_locks.at(row).unlock();      // unlock exclusive lock on row
    row_locks_mutex.unlock_shared(); // unlock shared lock on row locks
    tablet_logger.log("+OK Released exclusive row lock on R[" + row + "]", 20);
}

// add value at supplied row and column to tablet data
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

// delete row in tablet data
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

// delete value at supplied row and column in tablet data
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

// add value at supplied row and column to tablet data, ONLY if current value is val1
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

// // add value at supplied row and column to tablet data
// std::vector<char> Tablet::put_value(std::string &row, std::string &col, std::vector<char> &val)
// {
//     // row must be created if it does not exist in the map
//     if (data.count(row) == 0)
//     {
//         // acquire exclusive access to the row_locks map first to create a mutex for the new row
//         row_locks_mutex.lock();
//         row_locks[row];
//         row_locks_mutex.unlock();

//         row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks
//         row_locks.at(row).lock();      // acquire exclusive lock on data map to create row
//         // create entry for row in data map
//         data.emplace(row, std::unordered_map<std::string, std::vector<char>>());
//         tablet_logger.log("Created R[" + row + "]", 20);
//     }
//     else
//     {
//         row_locks_mutex.lock_shared(); // acquire shared lock on row_locks to read mutex from row_locks
//         row_locks.at(row).lock();      // acquire exclusive lock on data map to insert value
//     }

//     // at this point, the row we want to update has an exclusive lock on it
//     auto &row_level_data = data.at(row);

//     // add value at column
//     row_level_data[col] = val;

//     row_locks.at(row).unlock();      // unlock exclusive lock on row
//     row_locks_mutex.unlock_shared(); // unlock shared lock on row locks

//     tablet_logger.log("+OK Inserted value at R[" + row + "], C[" + col + "]", 20);
//     std::vector<char> response_msg(ok.begin(), ok.end());
//     return response_msg;
// }

// // delete row in tablet data
// std::vector<char> Tablet::delete_row(std::string &row)
// {
//     // Return empty vector if row not found in map
//     if (data.count(row) == 0)
//     {
//         tablet_logger.log("-ER Row not found", 20);
//         return construct_msg("Row not found", true);
//     }

//     row_locks_mutex.lock_shared(); // acquire a shared lock on row_locks to read from row_locks
//     row_locks.at(row).lock();      // acquire an exclusive lock on data map to delete row

//     // delete row
//     data.erase(row);

//     row_locks.at(row).unlock();      // unlock exclusive lock on row
//     row_locks_mutex.unlock_shared(); // release shared lock on row locks

//     // acquire exclusive access to the row_locks map to delete the mutex for the deleted row
//     row_locks_mutex.lock();
//     row_locks.erase(row);
//     row_locks_mutex.unlock();

//     tablet_logger.log("+OK Deleted R[" + row + "]", 20);
//     std::vector<char> response_msg(ok.begin(), ok.end());
//     return response_msg;
// }

// // delete value at supplied row and column in tablet data
// std::vector<char> Tablet::delete_value(std::string &row, std::string &col)
// {
//     // Return empty vector if row not found in map
//     if (data.count(row) == 0)
//     {
//         tablet_logger.log("-ER Row not found", 20);
//         return construct_msg("Row not found", true);
//     }

//     row_locks_mutex.lock_shared(); // acquire a shared lock on row_locks to read from row_locks
//     row_locks.at(row).lock();      // acquire an exclusive lock on data map to delete row

//     // at this point, the row we want to update has an exclusive lock on it
//     auto &row_level_data = data.at(row);

//     // delete value if row column exists
//     if (row_level_data.count(col) == 0)
//     {
//         row_locks.at(row).unlock();      // unlock exclusive lock on row
//         row_locks_mutex.unlock_shared(); // release shared lock on row locks
//         tablet_logger.log("-ER Column not found", 20);
//         return construct_msg("Column not found", true);
//     }

//     // delete value and associated column key
//     row_level_data.erase(col);

//     row_locks.at(row).unlock();      // unlock exclusive lock on row
//     row_locks_mutex.unlock_shared(); // release shared lock on row locks

//     tablet_logger.log("+OK Deleted value at R[" + row + "], C[" + col + "]", 20);
//     std::vector<char> response_msg(ok.begin(), ok.end());
//     return response_msg;
// }

// // add value at supplied row and column to tablet data, ONLY if current value is val1
// std::vector<char> Tablet::cond_put_value(std::string &row, std::string &col, std::vector<char> &curr_val, std::vector<char> &new_val)
// {
//     // get the value at the row and col - ensures we're not exclusively locking the row unless we're sure we need to write
//     const std::vector<char> &retrieved_val = get_value(row, col);

//     // exit if retrieved_val does not match curr_val (do preliminary size check first)
//     if (retrieved_val.size() != curr_val.size() || retrieved_val != curr_val)
//     {
//         tablet_logger.log("-ER V1 provided does not match currently stored value", 20);
//         return construct_msg("V1 provided does not match currently stored value", true);
//     }

//     // current value matches stored value
//     row_locks_mutex.lock_shared(); // acquire a shared lock on row_locks to read from row_locks
//     row_locks.at(row).lock();      // acquire an exclusive lock on data map to update value
//     auto &row_level_data = data.at(row);

//     // overwrite value at column with new value
//     row_level_data[col] = new_val;

//     row_locks.at(row).unlock();      // unlock exclusive lock on row
//     row_locks_mutex.unlock_shared(); // release shared lock on row locks
//     tablet_logger.log("+OK Conditionally inserted value at R[" + row + "], C[" + col + "]", 20);
//     std::vector<char> response_msg(ok.begin(), ok.end());
//     return response_msg;
// }
