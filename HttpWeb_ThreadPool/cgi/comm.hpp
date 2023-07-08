#pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>

void CutQuery(const std::string &target, const std::string sep, std::string &sub1_out, std::string &sub2_out)
{
    //切分字符串
    size_t pos = target.find(sep);
    if (pos != std::string::npos)
    {
        sub1_out = target.substr(0, pos);           // [0,pos)
        sub2_out = target.substr(pos + sep.size()); // [pos + 2, npos)
    }
}
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
    }

    return res;
}
