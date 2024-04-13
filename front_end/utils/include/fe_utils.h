#ifndef FE_UTILS_H
#define FE_UTILS_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>

// #include "../../../utils/include/utils.h"
#include "../../include/main.h"

// logger levels
const std::string SERVADDR = "127.0.0.1";
const int SERVPORT = 8000;

namespace FeUtils
{
    // creates socket, connects to kvs server and returns fd
    // @note: don't need s_addr and s_port for now, but can have default and overloaded value for auth?
    int open_socket(const std::string s_addr = SERVADDR, const int s_port= SERVPORT);

    // pass a fd and row, col values to perform GET(r,c), returns value
    std::vector<char> kv_get(int fd, std::vector<char> row, std::vector<char> col);

     // pass a fd and row to get all column values of row "RGET(r)"
    std::vector<char> kv_get_row(int fd, std::vector<char> row);

     // pass a fd and row, col, value to perform PUT(r,c,v)
    std::vector<char> kv_put(int fd, std::vector<char> row, std::vector<char> col, std::vector<char> val);

     // pass a fd and row, col, value1, value2 to perform CPUT(r,c,v1, v2)
    std::vector<char> kv_cput(int fd, std::vector<char> row, std::vector<char> col, std::vector<char> val1, std::vector<char> val2);

     // pass a fd and row, col to perform DELETE(r,c)
    std::vector<char> kv_del(int fd, std::vector<char> row, std::vector<char> col);

    // checks if a char vector starts with +OK
    bool kv_success(const std::vector<char>& vec);

}

#endif