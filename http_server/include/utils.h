#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

namespace Utils
{
    // split string by delimiter
    std::vector<std::string> split(std::string s, std::string delimiter);

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

    // ! this will be replaced by logger
    void error(std::string msg);
}

#endif