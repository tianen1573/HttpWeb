#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>

bool GetQueryString(std::string &query_string)
{
    bool res = true;
    std::string method = getenv("METHOD"); //请求方式

    if ("GET" == method) // get
    {
        query_string = getenv("QUERY_STRING");
    }
    else if ("POST" == method) // post
    {
        int content_length = atoi(getenv("CONTENT_LENGTH")); //当为post时，需要读取的正文长度
        char ch = 0;
        while (content_length)
        {
            ssize_t s = read(0, &ch, 1);

            if (s > 0)
            {
                query_string.push_back(ch);
                --content_length;
            }
            else if (s < 0)
            {
                res = false;
                break;
            }
            else
            {
                res = false;
                break;
            }
        }
    }
    else
    {
        res = false;
        ;
    }

    return res;
}

static void CutQuery(const std::string &target, const std::string sep, std::string &sub1_out, std::string &sub2_out)
{
    //切分字符串
    size_t pos = target.find(sep);
    if (pos != std::string::npos)
    {
        sub1_out = target.substr(0, pos);           // [0,pos)
        sub2_out = target.substr(pos + sep.size()); // [pos + 2, npos)
    }
}

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
        std::cout << name1 << " : " << value1 << std::endl;
        std::cout << name2 << " : " << value2 << std::endl;
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
