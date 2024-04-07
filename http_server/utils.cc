#include <iostream>
#include <algorithm>    // std::transform

#include "utils.h"

std::vector<std::string> Utils::split(std::string s, std::string delimiter) 
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


std::string Utils::l_trim(std::string s)
{
    size_t i = 0;
    while (i < s.length() && std::isspace(s[i])) {
        i++;
    }
    return s.substr(i);
}


std::string Utils::r_trim(std::string s)
{
    size_t i = s.length() - 1;
    while (i >= 0 && std::isspace(s[i])) {
        i--;
    }
    return s.substr(0, i + 1);
}


std::string Utils::trim(std::string s)
{
    return r_trim(l_trim(s));
}


std::string to_uppercase(std::string s) 
{   
    for (char& c : s) {
        c = std::toupper(c);
    }
    return s;
}


std::string to_lowercase(std::string s)
{
    for (char& c : s) {
        c = std::tolower(c);
    }
    return s;
}


void Utils::error(std::string msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}