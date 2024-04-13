/*
 * authentication.cc
 *
 *  Created on: Apr 11, 2024
 *      Author: peter-ai
 */

#include <cstring>
#include <openssl/sha.h>
#include <iostream>
#include <string>
#include <vector>
#include <random>

void sha256(char *string, char outputBuffer[65])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256_hash;
    SHA256_Init(&sha256_hash);
    SHA256_Update(&sha256_hash, string, strlen(string));
    SHA256_Final(hash, &sha256_hash);
    int i = 0;
    for(i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0;
}

std::string random_string(std::size_t length)
{
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;

    for (std::size_t i = 0; i < length; ++i)
    {
        random_string += CHARACTERS[distribution(generator)];
    }

    return random_string;
}

std::string generate_sid()
{
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, 9);

    std::string sid;

    for (int i=0; i < 64; i++)
    {
        sid += std::to_string(distribution(generator));
    }    

    return sid;
}

int main()
{   
    std::string msg1 = "hi my name is P";
    std::string msg2 = "Hi my name is P";
    std::string msg3 = "hi my name is A";
    std::string msg4 = "hi my name is M";
    
    std::vector<char> buf(65);
    sha256(&msg1[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg2[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg3[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg4[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg4[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg3[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg2[0], buf.data());
    std::cout << buf.data() << std::endl;

    sha256(&msg1[0], buf.data());
    std::cout << buf.data() << std::endl;

    std::cout << std::endl << "Generate Random Strings" << std::endl;
    std::cout << random_string(64) << std::endl;
    std::cout << random_string(64) << std::endl;

    std::cout << std::endl << "Generate Random SIDs" << std::endl;
    std::cout << generate_sid() << std::endl;
    std::cout << generate_sid() << std::endl;
    std::cout << generate_sid() << std::endl;

    return 0;
}