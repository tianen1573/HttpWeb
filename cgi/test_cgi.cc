#include <iostream>
#include <string>
#include <cstdlib>

int main()
{

    std::cerr << ".................................................." << std::endl;
    
    std::string method = getenv("METHOD");
    std::cerr << "Debug method: " <<  method << std::endl;

    std::string query_string;
    if("GET" == method)
    {
        query_string = getenv("QUERY_STRING");
        std::cerr << "Debug query_string: " <<  query_string << std::endl;
    }
    else if ("POST" == method) // post
    {

    }
    else
    {
        ;
    }

    std::cerr << "++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
    return 0;
    // std::string method_env = 
}