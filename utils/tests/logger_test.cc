/*
 * authentication.cc
 *
 *  Created on: Apr 09, 2024
 *      Author: peter-ai
 */

#include "../include/utils.h"
#include <iostream>

int main(int argc, char *argv[])
{
    std::string msg = "Hi";
    Logger test_logger("Tester");

    std::cerr << "Testing Logger" << std::endl;
    std::cerr << "--------------" << std::endl;
    for (int i=1; i <= 5; i++) 
    {
        switch (i)
        {
            case 1: 
                test_logger.log(msg, LOGGER_DEBUG);
                break;
            case 2: 
                test_logger.log(msg, LOGGER_INFO);
                break;
            case 3: 
                test_logger.log(msg, LOGGER_WARN);
                break;
            case 4: 
                test_logger.log(msg, LOGGER_ERROR);
                break;
            case 5: 
                test_logger.log(msg, LOGGER_CRITICAL);
                break;
        }
    }

    return 0;
}