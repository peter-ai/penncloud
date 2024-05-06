#pragma once

#include <string>

struct EmailData {
    std::string UIDL;
    std::string time;
    std::string to;
    std::string from;
    std::string subject;
    std::string body;
    std::string oldBody;
};
