#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <string>

#include "Util.hpp"
#include "Log.hpp"

//Http请求
class HttpRequest
{
public:
    std::string request_line;
    std::vector<std::string> request_header;
    std::string blank;
    std::string request_body;
};
//Http响应
class HttpResponse
{
public:
    std::string responce_line;
    std::vector<std::string> responce_header;
    std::string blank;
    std::string responce_body;
};

//读取请求，分析请求，构建响应
class EndPoint
{
public:
    EndPoint(int sock)
        : _sock(sock)
    {
    }
    ~EndPoint()
    {
    }

public:
    void RcvHttpRequest() //读取请求
    {
    }
    void ParseHttpRequest() //解析请求
    {
    }
    void BulidHttpResponse() //构建响应
    {
    }
    void SendHttpResponse() //发送响应
    {
    }

private:
    void RecvHttpRequestLine()
    {
        //报头
        Util::ReadLine(_sock, _http_request.request_line);
    }
    void RecvHttpRequestHeader()
    {
        //数据段
    }

private:
    int _sock;
    HttpRequest _http_request;
    HttpResponse _http_responce;
};

//HttpServer 处理请求类
class Entrance
{
public:
    static void *HandlerRequest(void *psock)
    {
        LOG(INFO, "Hander Request begin ...");
        int sock = *(int *)psock;
        delete (int *)psock;

        std::cout << "get a new link... : " << sock << std::endl;

#ifdef DEBUG
        char buffer[4096];
        recv(sock, buffer, sizeof(buffer), 0);
        std::cout << "----------------------begin--------------------------" << std::endl;
        std::cout << buffer << std::endl;
        std::cout << "----------------------end--------------------------" << std::endl;
#else
        EndPoint *ep = new EndPoint(sock);
        ep->RcvHttpRequest();
        ep->ParseHttpRequest();
        ep->BulidHttpResponse();
        ep->SendHttpResponse();

        delete ep;
#endif

        // 历史测试
        // std::string line;
        // Util::ReadLine(sock, line);
        // std::cout << line << std::endl;

        // close(sock);
        LOG(INFO, "Hander Request end.");
        return nullptr;
    }
};