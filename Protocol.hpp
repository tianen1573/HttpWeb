#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <sstream>
#include <algorithm>

#include "Util.hpp"
#include "Log.hpp"

// #define DEBUG 1

#define SEP ": "
#define WEB_ROOT "wwwroot"
#define HOME_PAGE "index.html"
#define HTTP_VERSION "HTTP/1.0"
#define LINE_END "\r\n"

#define OK 200
#define NOT_FOUND 404

static std::string Code2Desc(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
    case 404:
        return "Not Found";
    default:
        break;
    }
    return "";
}

//Http请求
class HttpRequest
{
public:
    // 请求
    std::string request_line;
    std::vector<std::string> request_header;
    std::string blank;
    std::string request_body; // POST正文
    std::string query_string; // GET正文

    // 解析结果
    std::string method;  // 请求方法
    std::string uri;     // 请求资源 path?args
    std::string version; // HTTP协议版本

    // 其他数据
    std::unordered_map<std::string, std::string> header_kv; //报头kv
    int content_length;                                     // 正文长度
    std::string path;                                       // 路径
    bool cgi;

public:
    HttpRequest()
        : content_length(0), cgi(false)
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
    int src_fd; //资源fd
    int src_size; //资源大小
public:
    HttpResponse()
        : blank(LINE_END), status_code(0), src_fd(-1), src_size(0)
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

        struct stat st; //写网页挺重要
        int src_size = 0;

        auto &code = _http_responce.status_code;
        code = OK; //默认为OK
        //判断请求方式
        if ("GET" != _http_request.method && "POST" != _http_request.method)
        {
            // 非法请求方式
            LOG(WARNING, "method is not right!");
            code = NOT_FOUND;
            goto END;
        }
        if ("GET" == _http_request.method)
        {
            ssize_t pos = _http_request.uri.find('?');
            if (pos != std::string::npos)
            {
                //带参
                Util::CutString(_http_request.uri, _http_request.path, _http_request.query_string, "?");
                _http_request.cgi = true;
            }
            else
            {
                //不带参
                _http_request.path = _http_request.uri;
            }
        }
        else if ("POST" == _http_request.method)
        {
            _http_request.cgi = true;
        }
        else
        {
            ;
        }

        // 判断路径合法性
        //如何保证path的合法性？ 1. /代表什么 2. 路径对应的资源 是否在服务器上存在
        //1. / 不一定代表根目录， 一般情况下 会指明web根目录：wwwroot，并且还需要默认首页，则/a/b/c ->> webroot/a/b/c
        //2. 确定资源是否存在， 确认wwwroot目录下， 某个文件是否存在， 使用系统调用stat

        //1.路径添加前缀
        _http_request.path = WEB_ROOT + _http_request.path;
        //判断是否为wwwroot
        if ("wwwroot/" == _http_request.path)
        {
            _http_request.path += HOME_PAGE;
        }
        //2. 判断资源是否合法
        // struct stat st;  //-- 写在goto前 //写网页挺重要
        // int src_size = 0; //-- 写在goto前
        if (stat(_http_request.path.c_str(), &st) == 0)
        {
            //a. 存在
            //访问的路径不一定是一个确定的文件，可能是一个路径，所以一般情况下，每个目录下都会构建一个默认访问文件，访问该目录时，即访问这个文件
            //判断路径是否是目录
            if (S_ISDIR(st.st_mode))
            {
                //b. 是目录
                _http_request.path += "/";
                _http_request.path += HOME_PAGE;
                stat(_http_request.path.c_str(), &st); // 更新st
            }
            //判断资源是否是可执行文件
            //man 2 stat
            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            {
                //c. 可执行文件
                _http_request.cgi = true;
            }
            src_size = st.st_size; //文件大小
        }
        else
        {
            //a. 不存在
            std::string info = _http_request.path;
            info += " Not Found!";
            LOG(WARNING, info);
            code = NOT_FOUND;
            goto END;
        }

        //处理方式 1.cgi 2. no cgi
        if (_http_request.cgi)
        {
            // post/get+带参，可执行文件
            ProcessCgi();
        }
        else
        {
            //简单返回 静态网页
            code = ProcessNonCgi(src_size);
        }

#ifdef DEBUG
        LOG(DEBUG, _http_request.method);
        LOG(DEBUG, _http_request.path);
        LOG(DEBUG, _http_request.version);
#endif

    END:
        if (OK != code)
        {
            ;
        }
    }
    void SendHttpResponse() //发送响应
    {
        send(_sock, _http_responce.responce_line.c_str(), _http_responce.responce_line.size(), 0); //状态行
        for (auto iter : _http_responce.responce_header)                                           //报头
        {
            send(_sock, iter.c_str(), iter.size(), 0);
        }
        send(_sock, _http_responce.blank.c_str(), _http_responce.blank.size(), 0); //空行
        sendfile(_sock, _http_responce.src_fd, nullptr, _http_responce.src_size);  //正文
        close(_http_responce.src_fd);
    }

private:
    void RecvHttpRequestLine()
    {
        //请求行
        Util::ReadLine(_sock, _http_request.request_line);
        _http_request.request_line.resize(_http_request.request_line.size() - 1);

#ifdef DEBUG
        LOG(DEBUG, _http_request.request_line);
#endif
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

#ifdef DEBUG
            LOG(DEBUG, line);
#endif
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
                ssize_t s = recv(_sock, &ch, 1, 0);
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

        //把method转换成大写格式， 避免post/get出错
        // transform
        std::transform(_http_request.method.begin(), _http_request.method.end(), _http_request.method.begin(), ::toupper); //转大写

#ifdef DEBUG
        LOG(DEBUG, _http_request.method);
        LOG(DEBUG, _http_request.uri);
        LOG(DEBUG, _http_request.version);
#endif 
    }
    void ParseHttpRequestHeader()
    {
        //解析请求报头
        std::string key, value;

        for (auto &str : _http_request.request_header)
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
                _http_request.content_length = std::stoi(iter->second);
                return true;
            }
        }

        return false;
    }
    int ProcessNonCgi(int src_size)
    {
        //预处理正文 -- 打开静态网页文件
        _http_responce.src_fd = open(_http_request.path.c_str(), O_RDONLY);
        if (_http_responce.src_fd >= 0)
        {
            // 处理状态行
            _http_responce.responce_line = HTTP_VERSION;
            _http_responce.responce_line += " ";
            _http_responce.responce_line += std::to_string(_http_responce.status_code);
            _http_responce.responce_line += " ";
            _http_responce.responce_line += Code2Desc(_http_responce.status_code);
            _http_responce.responce_line += LINE_END;

#ifdef DEBUG
            LOG(DEBUG, _http_responce.responce_line);
#endif

            // 处理响应报头

            // 处理空行 -- 已处理

            //读取正文
            //opne, sendfile
            _http_responce.src_size = src_size;//正文大小
            return OK;
        }
        else
        {
            LOG(INFO, "scr_file open error!");
        }

        return NOT_FOUND;
    }
    int ProcessCgi()
    {
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