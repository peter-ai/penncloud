#pragma once

#include <string>

// UIDL: time, to, from, subject
// EMAIL FORMAT //
// time: Fri Mar 15 18:47:23 2024\n
// to: recipient@example.com\n
// from: sender@example.com\n
// subject: Your Subject Here\n
// body: Hello, this is the body of the email.\n
// oldBody: ____

struct EmailData {
    std::string UIDL;
    std::string time;
    std::string to;
    std::string from;
    std::string subject;
    std::string body;
    std::string oldBody;
};
