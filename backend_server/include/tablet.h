#ifndef TABLET_H
#define TABLET_H

#include <string>
#include <map>
#include <unordered_map>
#include <shared_mutex>

class Tablet 
{
// fields
public:
    // NOTE: if the key range is "aa" to "bz", this tablet will manage every key UP TO AND INCLUDING "bz"
    // For example, a key called bzzzz would be managed in this server. The next server would start at "ca" 
    std::string range_start;     // start of key range managed by this backend server
    // ! might not need to store end of range
    std::string range_end;       // end of key range managed by this backend server

private:
    // In memory representation of a tablet's data
    // Map of row key (string) -> column key (string) -> value (vector<char>)
    std::map<std::string, std::unordered_map<std::string, std::vector<char>>> data;

    // enables row level locking
    // read-write locking (a row has concurrent read access, exclusive write access)
    std::map<std::string, std::shared_timed_mutex> row_locks;

    // locks access to the row_locks map while a new lock is being created for a row
    std::shared_timed_mutex row_locks_mutex;

// methods
public:
    // Construct to initialize a tablet
    Tablet(std::string range_start, std::string range_end) 
            : range_start(range_start), range_end(range_end), data(), row_locks() {}

    // default constructor
    // NOTE: this should ONLY be used during deserialization
    // to deserialize, create a tablet using this constructor and then call deserialize on the instance to populate the object's fields
    // deserialize will populate the tablet's fields
    Tablet() : range_start(""), range_end(""), data(), row_locks() {}

    /**
     *  Methods for interacting with in memory data
    */

    // read all columns from tablet data
    std::vector<std::string> get_row(std::string& row);

    // read value from tablet data
    std::vector<char> get_value(std::string& row, std::string& col);

    // add value at supplied row and column to tablet data
    // this operation acquires exclusive access to the row
    // if the row does not exist, both the row and its mutex will be created
    // if column does not exist, it will be created
    void put_value(std::string& row, std::string& col, std::vector<char>& val);     

    // add value at supplied row and column to tablet data, ONLY if current value is val1
    // this operation acquires exclusive access to the row
    // if the row or column does not exist, they will NOT be created
    void cond_put_value(std::string& row, std::string& col, std::vector<char>& old_val, std::vector<char>& new_val);  

    // delete value at supplied row and column in tablet data
    // this operation requires exclusive access to the row
    // nothing will happen if the row and column do not exist
    void delete_value(std::string& row, std::string& col); 

    // delete row in tablet data
    // this operation requires exclusive access to the row
    // nothing will happen if the row does not exist
    void delete_row(std::string& row); 

    /**
     * Serialization methods
    */
    // ! note for serialization - don't need to serialize row locks, since we can construct one for each row on deserialization
    void serialize(const std::string& file_name);   // serialize tablet into a file called file_name
    void deserialize(const std::string& file_name); // deserialize file_name into this tablet object
};

#endif