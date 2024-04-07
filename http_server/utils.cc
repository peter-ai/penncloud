#include <iostream>

#include "utils.h"

std::vector<std::string> Utils::parse_string(std::string s, std::string delimiter) 
{
    std::vector<std::string> lines;
    size_t pos = 0;
    std::string line;

    // process lines according to delimiter 
    while ((pos = s.find(delimiter)) != std::string::npos) {
        line = s.substr(0, pos);
        lines.push_back(line);
        s.erase(0, pos + delimiter.length());
    }

    // Push the remaining part of the string as the last element
    if (!s.empty()) {
        lines.push_back(s);
    }

    return lines;
}

// ! trim s
std::string Utils::trim(std::string s)
{

}

void Utils::error(std::string msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}