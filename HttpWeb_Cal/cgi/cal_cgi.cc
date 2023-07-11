#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>

#include "comm.hpp"
#include "Cal.hpp"

//计算器程序
int main()
{
    std::cerr << ".................................................." << std::endl;

    std::string query_string; //CalString=1+2+3
    std::string reslut;
    if (GetQueryString(query_string))
    {
        std::string name;
        std::string expr;
        bool ok = true;
        CutQuery(query_string, "=", name, expr);
        expr = UrlDecode(expr, ok);
        if (ok == false)
        {
            std::cerr << "解码失败：参数出现错误！" << std::endl;
            return 1;
        }
        else
        {
            Cal cal(expr);
            reslut = expr;
            if (cal.LexicalAnalysis() == false)
            {
                if (cal.Prase() == false)
                {
                    cal.CalExpr();
                    if (cal.IsZero())
                    {
                        reslut += " Divide by zero in expression";
                    }
                    else
                    {
                        reslut += " = " + std::to_string(cal.Reslut());
                    }
                }
                else
                {
                    reslut += " is wrong expression!";
                }
            }
            else
            {
                reslut += " has word errors!";
            }
            // reslut = UrlEncode(reslut);

            //fd: 1
            std::cout << "<html>";
            std::cout << "<head><meta charset=\"utf-8\"></head>";
            std::cout << "<body>";
            std::cout << "<h3>" << reslut << "</h3>";
            std::cout << "</body>";
            std::cout << "</html>";
        }
    }
    else
    {
        std::cerr << "错误的读取方式：服务器读取请求/解析请求/设置环境变量时出错!" << std::endl;
        return 1;
    }

    std::cerr << reslut << std::endl;
    std::cerr << ".................................................." << std::endl;
    return 0;
}
