#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>

#include "comm.hpp"

//计算器程序
int main()
{
    std::cerr << ".................................................." << std::endl;

    std::string query_string; //a=100&b=200
    if (GetQueryString(query_string))
    {
        std::string str1;
        std::string str2;
        CutQuery(query_string, "&", str1, str2);

        std::string name1;
        std::string value1;
        CutQuery(str1, "=", name1, value1);
        std::string name2;
        std::string value2;
        CutQuery(str2, "=", name2, value2);

        //fd: 1
        std::cout << "<html>";
        std::cout << "<head><meta charset=\"utf-8\"></head>";
        std::cout << "<body>";
        std::cout << "<h3>" << name1 << " : " << value1 << "</h3>";
        std::cout << "<h3>" << name2 << " : " << value2 << "</h3>";
        std::cout << "</body>";
        std::cout << "</html>";


        //fd: 2
        std::cerr << name1 << " : " << value1 << std::endl;
        std::cerr << name2 << " : " << value2 << std::endl;
    }
    else
    {
        ;
    }

    std::cerr << "++++++++++++++++++++++++++++++++++++++++++++++++++" << std::endl;
    return 0;
}
