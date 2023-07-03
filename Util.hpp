#pragma once

#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>

//工具类
class Util
{
public:
    static int ReadLine(int sock, std::string &out)
    {
        char ch = 'X';
        while (ch != '\n')
        {
            ssize_t s = recv(sock, &ch, 1, 0);
            if(s > 0)
            {
                if(ch == '\r')
                {
                    // \r -> \n, \r\n -> \n
                    recv(sock, &ch, 1, MSG_PEEK);//窥探，看看后一个字符是啥
                    if(ch == '\n')
                    {
                        //\r\n
                        recv(sock, &ch, 1, 0); //读取掉\n,覆盖ch
                    }
                    else
                    {
                        //\r
                        ch = '\n';//手动覆盖
                    }
                }

                //1. 普通字符
                //2. '\n'
                out.push_back(ch);
            }
            else if (s == 0)
            {

            }
            else
            {

            }
        }

        return out.size();
    }
    static bool CutString(const std::string &target, std::string &sub1_out, std::string &sub2_out, const std::string sep)
    {
        //切分字符串

        size_t pos = target.find(sep);
        if(pos != std::string::npos)
        {
            sub1_out = target.substr(0, pos); // [0,pos)
            sub2_out = target.substr(pos + sep.size()); // [pos + 2, npos)
            return true;
        }
        return false;
    }
};