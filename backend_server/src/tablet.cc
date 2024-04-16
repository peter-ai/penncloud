#include <iostream>

#include "../include/tablet.h"

// read all columns from tablet data at supplied row
std::vector<std::string> Tablet::get_row(std::string& row)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0) {
        return std::vector<std::string>(); 
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();

    // acquire a shared lock on this row's mutex
    // if lock cannot be acquired, thread should wait here until lock can be acquired
    // ! check the logic here - make sure we're waiting here if a shared lock can't be acquired
    row_locks.at(row).lock_shared();
    const auto& row_level_data = data.at(row);

    // iterate and store all rows
    std::vector<std::string> cols;
    for (const auto& entry : row_level_data) {
        cols.push_back(entry.first);
    }

    // release shared lock on row
    row_locks.at(row).unlock_shared();

    // release shared lock on row_lock
    row_locks_mutex.unlock_shared();

    return cols;
}


// ! what if the value at that row + col is just empty? Should we differentiate between a successful/unsuccessful operation?
// read value at supplied row and column from tablet data
std::vector<char> Tablet::get_value(std::string& row, std::string& col)
{
    // Return empty vector if row not found in map
    if (data.count(row) == 0) {
        return std::vector<char>(); 
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();

    // otherwise, acquire a shared lock on this row's mutex
    // if lock cannot be acquired, thread should wait here until lock can be acquired
    // ! check the logic here - make sure we're waiting here if a shared lock can't be acquired
    row_locks.at(row).lock_shared();
    const auto& row_level_data = data.at(row);

    // release shared lock and exit if col not found in row
    if (row_level_data.count(col) == 0) {
        // release shared lock on row
        row_locks.at(row).unlock_shared();

        // release shared lock on row_lock
        row_locks_mutex.unlock_shared();

        return std::vector<char>(); 
    }

    // retrieve value from row
    std::vector<char> value = row_level_data.at(col);

    // release shared lock on row
    row_locks.at(row).unlock_shared();

    // release shared lock on row_lock
    row_locks_mutex.unlock_shared();

    return value;
}


// add value at supplied row and column to tablet data
void Tablet::put_value(std::string& row, std::string& col, std::vector<char>& val) 
{
    // check if the row exists in the map
    if (data.count(row) == 0) {
        // row must be created
        
        // acquire exclusive access to the row_locks map first to create a mutex for the new row
        row_locks_mutex.lock();
        row_locks[row]; // ! this creates a mutex at row (double check this)
        row_locks_mutex.unlock();

        // acquire a shared lock on row_locks since we're just reading from it now
        // ! figure out how to make sure shared lock blocks to acquire the shared lock
        row_locks_mutex.lock_shared();

        // acquire exclusive lock on data map to create row
        row_locks.at(row).lock();
        // create entry for row in data map
        data.emplace(row, std::unordered_map<std::string, std::vector<char>>());
    } else {
        // acquire an exclusive lock on data map to put value
        row_locks.at(row).lock(); 
    }
    
    // at this point, the row we want to update has an exclusive lock on it
    auto& row_level_data = data.at(row);
    
    // add value at column
    // ! NOTE - this overwrites the value 
    row_level_data[col] = val;

    // unlock exclusive lock on row
    row_locks.at(row).unlock();

    // release shared lock on row locks
    row_locks_mutex.unlock_shared();
}


// add value at supplied row and column to tablet data, ONLY if current value is val1
void Tablet::cond_put_value(std::string& row, std::string& col, std::vector<char>& old_val, std::vector<char>& new_val) 
{
    // get the value at the row and col
    // this ensures we're not exclusively locking unless we're sure we need to write
    const std::vector<char>& retrieved_val = get_value(row, col);

    // exit if retrieved_val does not match old_val
    if (retrieved_val.size() != old_val.size() || retrieved_val != old_val) {
        return;
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();

    // acquire exclusive access to row to conditionally update value
    row_locks.at(row).lock(); 
    auto& row_level_data = data.at(row);
    
    // overwrite value at column
    row_level_data[col] = new_val;

    // unlock exclusive lock on row
    row_locks.at(row).unlock();

    // release shared lock on row locks
    row_locks_mutex.unlock_shared();
}


// delete value at supplied row and column in tablet data
void Tablet::delete_value(std::string& row, std::string& col) 
{
    // exit if row does not exist in map
    if (data.count(row) == 0) {
        return;
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();
    
    // acquire an exclusive lock on data map to delete value
    row_locks.at(row).lock(); 

    // at this point, the row we want to update has an exclusive lock on it
    auto& row_level_data = data.at(row);
    
    // delete value if row column exists
    if (row_level_data.count(col) != 0) {
        // delete value and associated column key
        row_level_data.erase(col);
    }

    // unlock exclusive lock on row
    row_locks.at(row).unlock();

    // release shared lock on row locks
    row_locks_mutex.unlock_shared();
}


// delete row in tablet data
void Tablet::delete_row(std::string& row) 
{
    // exit if row does not exist in map
    if (data.count(row) == 0) {
        return;
    }

    // acquire a shared lock on row_locks to read from row_locks
    // ! figure out how to make sure shared lock blocks to acquire the shared lock
    row_locks_mutex.lock_shared();
    
    // acquire an exclusive lock on data map to delete row
    row_locks.at(row).lock(); 

    // delete row
    data.erase(row);

    // unlock exclusive lock on row
    row_locks.at(row).unlock();

    // release shared lock on row locks
    row_locks_mutex.unlock_shared();

    // acquire exclusive access to the row_locks map first to delete the mutex for the deleted row
    row_locks_mutex.lock();
    row_locks.erase(row);
    row_locks_mutex.unlock();
}