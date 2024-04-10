#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

// logger levels
constexpr int LOGGER_DEBUG=10;
constexpr int LOGGER_INFO=20;
constexpr int LOGGER_WARN=30;
constexpr int LOGGER_ERROR=40;
constexpr int LOGGER_CRITICAL=50;

namespace Utils
{
    // split string on all occurrences of delimiter, with repeated instances of delimiter
    std::vector<std::string> split(std::string s, std::string delimiter);

    // split string only on first occurrence of delimiter
    std::vector<std::string> split_on_first_delim(std::string s, std::string delimiter);

    // trim whitespace from left side of string
    std::string l_trim(std::string s);

    // trim whitespace from right side of string
    std::string r_trim(std::string s);

    // trim whitespace from string on both sides
    std::string trim(std::string s);

    // convert string to uppercase
    std::string to_uppercase(std::string s);

    // convert string to lowercase
    std::string to_lowercase(std::string s);

    // convert and return the current utc time
    std::string get_utc_time();
}

// class that logs messages at various levels
class Logger
{
// fields
public:
    // name of this logger instance
    std::string name; 

// methods
public:
    // logger initialized with an associated name
    Logger(std::string name);

    // disable default constructor - Logger should only be created with an associated fd
    Logger() = delete;

    ~Logger()
    {
    }  

    // output logging message
    void log(std::string message, int level); 
};

#endif