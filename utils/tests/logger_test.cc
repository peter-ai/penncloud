#include "../include/utils.h"
#include <iostream>

int main(int argc, char *argv[])
{
    std::string msg = "Hi";
    Logger test_logger("Tester");

    std::cerr << "Testing Logger" << std::endl;
    std::cerr << "--------------" << std::endl;
    for (int i=1; i <= 5; i++) {test_logger.log(msg, i*10);}

    return 0;
}