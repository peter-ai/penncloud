#include <iostream>
#include <algorithm>    // std::transform
#include <sys/time.h>

#include "../include/utils.h"

// ! test this for something like GET   some_path HTTP/1.1 (with trailing spaces)  
// ! maybe add a method for single delimiter split and repeat delimiter
std::vector<std::string> Utils::split(std::string s, std::string delimiter) 
{
    std::vector<std::string> lines;
    size_t pos = 0;
    std::string line;

    // process lines according to delimiter 
    while ((pos = s.find(delimiter)) != std::string::npos) {
        line = s.substr(0, pos);
        // should handle consecutive delimiters
        if (line.length() != 0) {
            lines.push_back(line);
        }
        s.erase(0, pos + delimiter.length());
    }

    // Push the remaining part of the string as the last element
    if (!s.empty()) {
        lines.push_back(s);
    }

    return lines;
}

std::vector<std::string> Utils::split_on_first_delim(std::string s, std::string delimiter) 
{
    std::vector<std::string> tokens;

    size_t pos = s.find(delimiter);
    if (pos != std::string::npos) {
        std::string token = s.substr(0, pos);
        if (token.empty()) {
            tokens.push_back(token);
        }
        s.erase(0, pos + delimiter.length());
    }

    // Push the remaining part of the string as the last element
    if (!s.empty()) {
        tokens.push_back(s);
    }

    return tokens;
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
    int i = s.length() - 1;
    while (i >= 0 && std::isspace(s[i])) {
        i--;
    }
    return s.substr(0, i + 1);
}


std::string Utils::trim(std::string s)
{
    return r_trim(l_trim(s));
}


std::string Utils::to_uppercase(std::string s) 
{   
    for (char& c : s) {
        c = std::toupper(c);
    }
    return s;
}


std::string Utils::to_lowercase(std::string s)
{
    for (char& c : s) {
        c = std::tolower(c);
    }
    return s;
}


std::string Utils::get_utc_time()
{
  // get time
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm *utc = gmtime(&(tv.tv_sec));

  // format time to hh:mm:ss:uuuuuu
  std::string hr = std::to_string(utc->tm_hour);
  std::string utc_time = (hr.size() == 1 ? "0" + hr : hr) + ":" + std::to_string(utc->tm_min) + ":" + std::to_string(utc->tm_sec) + "." + std::to_string(tv.tv_usec).substr(0, 6);

  // return utc time
  return utc_time;
}

Logger::Logger(std::string name): name(name) {}

void Logger::log(std::string message, int level)
{
    // set time and info
    std::string output = "[" + Logger::name + " - " + Utils::get_utc_time() + "] ";
    
    // set logging severity level
    if (level == LOGGER_DEBUG) 
    {
        output += "DEBUG: ";
    }
    else if (level == LOGGER_INFO) 
    {
        output += "INFO: ";
    }
    else if (level == LOGGER_WARN) 
    {
        output += "WARNING: ";
    }
    else if (level == LOGGER_ERROR) 
    {
        output += "ERROR: ";
    }
    else 
    {
        output += "CRITICAL: ";
    }

    // add logging message and output log message
    output += message;
    std::cerr << output << std::endl;
}