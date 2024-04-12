#include "../include/fe_utils.h"
#include <iostream>

void print_vector_as_string(const std::vector<char> &vec)
{
    std::string str(vec.begin(), vec.end());
    std::cout << "Vector as string: " << str << std::endl;
}

int main(int argc, char *argv[])
{
    int sockfd = FeUtils::open_socket();
    std::cout << "sockfd: " << sockfd << std::endl;

    std::vector<char> response;
    std::vector<char> row = {'r', 'o', 'w'};
    std::vector<char> col = {'c', 'o', 'l'};
    std::vector<char> val1 = {'v', 'a', 'l', '1'};
    std::vector<char> val2 = {'v', 'a', 'l', '2'};


std::cout << "kv get " << std::endl;
    response = FeUtils::kv_get(sockfd, row, col);
    print_vector_as_string(response);

    std::cout << "kv fet done" << std::endl;

     std::cout << "kv get row" << std::endl;

    response = FeUtils::kv_get_row(sockfd, row);
    
    print_vector_as_string(response);

     std::cout << "kv get row  done" << std::endl;

      std::cout << "kv put" << std::endl;
    response = FeUtils::kv_put(sockfd, row, col, val1);
    print_vector_as_string(response);

     std::cout << "kv put done" << std::endl;

        std::cout << "kv cput" << std::endl;
    response = FeUtils::kv_cput(sockfd, row, col, val1, val2);
    print_vector_as_string(response);
     std::cout << "kv cput done" << std::endl;

        std::cout << "kv del " << std::endl;
    response = FeUtils::kv_del(sockfd, row, col);
    print_vector_as_string(response);

     std::cout << "kv del done" << std::endl;

    close(sockfd);

    std::cout << "sockfd closed " << std::endl;
}