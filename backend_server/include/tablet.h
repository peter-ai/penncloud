#ifndef TABLET_H
#define TABLET_H

#include <string>
#include <map>
#include <unordered_map>
#include <shared_mutex>

#include "../../utils/include/utils.h"

class Tablet
{
    // fields
public:
    std::string range_start;  // start of key range managed by this tablet
    std::string range_end;    // end of key range managed by this tablet
    std::string log_filename; // name of log file for this tablet

private:
    static const char delimiter;  // delimiter used to separate components of tablet command
    static const std::string ok;  // "+OK" (for success messages)
    static const std::string err; // "-ER" (for error messages)

    // row key (string) -> column key (string) -> value (vector<char>)
    std::map<std::string, std::unordered_map<std::string, std::vector<char>>> data; // In-memory representation of a tablet
    std::unordered_map<std::string, std::shared_timed_mutex> row_locks;             // row-level read-write locks for tablet data
    std::shared_timed_mutex row_locks_mutex;                                        // read-write lock for row_locks map

    // methods
public:
    // Constructor to initialize a tablet - used on server start up
    Tablet(std::string range_start, std::string range_end)
        : range_start(range_start), range_end(range_end), log_filename(range_start + "_" + range_end + "_log"), data(), row_locks(), row_locks_mutex() {}

    // Default constructor
    // This should ONLY be used when deserializing a file into a tablet.
    // To deserialize a file, initialize a tablet using this default constructor, then call deserialize on the tablet to populate its fields
    Tablet() : range_start(""), range_end(""), log_filename(""), data(), row_locks(), row_locks_mutex() {}

    /**
     *  READ-ONLY METHODS
     */

    std::vector<char> get_all_rows();                                // read all rows in tablet (rows are separated by delimiter)
    std::vector<char> get_row(std::string &row);                     // read all columns from tablet data (columns are separated by delimiter)
    std::vector<char> get_value(std::string &row, std::string &col); // read value from tablet data

    /**
     *  WRITE METHODS
     */

    // add value at supplied row and column to tablet data
    // this operation acquires exclusive access to the row
    // if the row does not exist, both the row and its mutex will be created
    // if column does not exist, it will be created
    std::vector<char> put_value(std::string &row, std::string &col, std::vector<char> &val);

    // add value at supplied row and column to tablet data, ONLY if current value is val1
    // this operation acquires exclusive access to the row
    // if the row or column does not exist, they will NOT be created
    std::vector<char> cond_put_value(std::string &row, std::string &col, std::vector<char> &curr_val, std::vector<char> &new_val);

    // delete row in tablet data
    // this operation requires exclusive access to the row
    // nothing will happen if the row does not exist
    std::vector<char> delete_row(std::string &row);

    // delete value at supplied row and column in tablet data
    // this operation requires exclusive access to the row
    // nothing will happen if the row and column do not exist
    std::vector<char> delete_value(std::string &row, std::string &col);

    // renames row from "old_row" to "new_row" in tablet data
    // this operation requires exclusive access to the row
    // nothing will happen if the row does not exist
    std::vector<char> rename_row(std::string &old_row, std::string &new_row);

    // renames column in supplied row from "old_col" to "new_col" in tablet data
    // this operation requires exclusive access to the row
    // nothing will happen if the row and column do not exist
    std::vector<char> rename_column(std::string &row, std::string &old_col, std::string &new_col);

    /**
     * ACQUIRING/RELEASING LOCKS FOR WRITE METHODS
     */

    // This operation MUST precede ALL write operations
    // Usage - called by primary server before sending PREP command, and by secondary server in response to PREP command
    int acquire_exclusive_row_lock(std::string &operation, std::string &row); // acquires shared lock on row_locks, exclusive lock on row

    // This operation is ONLY called during an ABRT operation
    // Write methods automatically release their locks upon completion of the write operation
    void release_exclusive_row_lock(std::string &row); // releases shared lock on row_locks and exclusive lock on row

    /**
     * SERIALIZATION METHODS
     */

    void serialize(const std::string &file_name);             // serialize tablet into a file called file_name
    void deserialize_from_file(const std::string &file_name); // deserialize file_name into this tablet object
    void deserialize_from_stream(std::vector<char> &stream);  // deserialize file_name into this tablet object

private:
    std::vector<char> construct_msg(const std::string &msg, bool error); // construct success/error msg to send back to client
};

#endif