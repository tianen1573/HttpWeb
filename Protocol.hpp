#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <string>
#include <sstream>

#include "Util.hpp"
#include "Log.hpp"

//Http请求
class HttpRequest
{
public:
    // 请求
    std::string request_line;
    std::vector<std::string> request_header;
    std::string blank;
    std::string request_body;

    // 解析结果
    std::string method; // 请求方法
    std::string uri; // 请求资源
    std::string version; // HTTP协议版本
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
        RecvHttpRequestLine(); // 读取请求行
        RecvHttpRequestHeader(); // 读取请求报头
        //正文
    }
    void ParseHttpRequest() //解析请求
    {
        ParseHttpRequestLine(); // 解析请求行
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
        //请求行
        Util::ReadLine(_sock, _http_request.request_line);
        _http_request.request_line.resize(_http_request.request_line.size() - 1);

        LOG(DEBUG, _http_request.request_line);
    }
    void RecvHttpRequestHeader()
    {
        //请求报头
        std::string line;
        while ("\n" != line)
        {
            line.clear();//清空
            Util::ReadLine(_sock, line);
            if("\n" == line)
            {
                _http_request.blank = line;
                break;
            }

            line.resize(line.size() - 1); //不需要最后一个'\n'
            _http_request.request_header.push_back(line);

            LOG(DEBUG, line);
        }

    }
    void ParseHttpRequestLine()
    {
        auto &line = _http_request.request_line;
        std::stringstream ss(line); // 流提取
        ss >> _http_request.method >> _http_request.uri >> _http_request.version;

        LOG(INFO, _http_request.method);
        LOG(INFO, _http_request.uri);
        LOG(INFO, _http_request.version);
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