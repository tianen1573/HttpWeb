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
    }
};