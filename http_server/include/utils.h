#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

namespace Utils
{
    std::vector<std::string> parse_string(std::string s, std::string delimiter);

    std::string trim(std::string s);

    void error(std::string msg);
}

#endif