#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <sstream>

#include "Util.hpp"
#include "Log.hpp"

#define SEP ": "
#define OK 200
#define NOT_FOUND 404

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
    std::string method;  // 请求方法
    std::string uri;     // 请求资源 path?args
    std::string version; // HTTP协议版本

    // 其他数据
    std::unordered_map<std::string, std::string> header_kv;
    int content_length;
    std::string path;
    std::string query_string;
public:
    HttpRequest()
        : content_length(0)
    {
    }
    ~HttpRequest()
    {
    }
};
//Http响应
class HttpResponse
{
public:
    std::string responce_line;
    std::vector<std::string> responce_header;
    std::string blank;
    std::string responce_body;

    int status_code;

public:
    HttpResponse()
        : status_code(0)
    {
    }
    ~HttpResponse()
    {
    }
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
        close(_sock);
    }

public:
    void RcvHttpRequest() //读取请求
    {
        RecvHttpRequestLine();   // 读取请求行
        RecvHttpRequestHeader(); // 读取请求报头
        RecvHttpRequestBody();   // 读取正文
    }
    void ParseHttpRequest() // 解析请求
    {
        ParseHttpRequestLine();   // 解析请求行
        ParseHttpRequestHeader(); // 解析请求报头
    }
    void BulidHttpResponse() //构建响应
    {
        auto &code = _http_responce.status_code;
        if ("GET" != _http_request.method && "POST" != _http_request.method)
        {
            // 非法请求
            LOG(WARNING, "method is not right!");
            code = NOT_FOUND;
            goto END;
        }
        if("GET" == _http_request.method)
        {
            ssize_t pos  = _http_request.uri.find('?');
            if(pos != std::string::npos)
            {
                Util::CutString(_http_request.uri, _http_request.path, _http_request.query_string, "?");
            }
            else
            {
                _http_request.path = _http_request.uri;
            }
        }

    END:
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
            line.clear(); //清空
            Util::ReadLine(_sock, line);
            if ("\n" == line)
            {
                _http_request.blank = line;
                break;
            }

            line.resize(line.size() - 1); //不需要最后一个'\n'
            _http_request.request_header.push_back(line);

            LOG(DEBUG, line);
        }
    }
    void RecvHttpRequestBody()
    {
        if (IsNeedRecvHttpRequestBody())
        {
            int content_length = _http_request.content_length;
            auto &body = _http_request.request_body;

            char ch = 0;
            while (content_length)
            {
                ssize_t s = recv(sock, &ch, 1, 0);
                if (s > 0)
                {
                    body.push_back(ch);
                    --content_length;
                }
                else if (s == 0)
                {
                    break;
                }
                else
                {
                    LOG(ERROR, "recv error!");
                    break;
                }
            }
        }
    }

    void ParseHttpRequestLine()
    {
        //解析请求行
        auto &line = _http_request.request_line;
        std::stringstream ss(line); // 流提取
        ss >> _http_request.method >> _http_request.uri >> _http_request.version;

        LOG(INFO, _http_request.method);
        LOG(INFO, _http_request.uri);
        LOG(INFO, _http_request.version);
    }
    void ParseHttpRequestHeader()
    {
        //解析请求报头
        std::string key, value;

        for (auto &str ： _http_request.request_header)
        {
            if (Util::CutString(str, key, value, SEP))
            {
                _http_request.header_kv.insert({key, value});
            }
        }
    }

    bool IsNeedRecvHttpRequestBody()
    {
        auto &method = _http_request.method;
        if ("POST" == method)
        {
            auto &header_kv = _http_request.header_kv;
            auto iter = header_kv.find("Content-Length");
            if (iter != header_kv.end())
            {
                _http_request.content_length = std::stoi(iter.second);
                return true;
            }
        }

        return false;
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
        std::cout << "----------------------end--------------------------\n" << std::endl;
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