#pragma once

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fstream>

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

unsigned char ToHex(unsigned char x)
{
	return  x > 9 ? x + 55 : x + 48;
}
unsigned char FromHex(unsigned char x, bool &ok)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else ok = false;
	return y;
}
std::string UrlEncode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ')
			strTemp += "+";
		else
		{
			strTemp += '%';
			strTemp += ToHex((unsigned char)str[i] >> 4);
			strTemp += ToHex((unsigned char)str[i] % 16);
		}
	}
	return strTemp;
}
std::string UrlDecode(const std::string& str, bool &ok)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		if (str[i] == '+') strTemp += ' ';
		else if (str[i] == '%')
		{
			if(i + 2 >= length)
            {
                ok = false;
                return "";
            }
			unsigned char high;
            unsigned char low;
            high = FromHex((unsigned char)str[++i], ok);
            if(ok)
            {
                low = FromHex((unsigned char)str[++i], ok);
                if(ok == false)
                {
                    return "";
                }
            }
            else
            {
                return "";
            }
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}