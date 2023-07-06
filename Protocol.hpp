#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
#define PAGE_HOME "index.html"
#define HTTP_VERSION "HTTP/1.0"
#define LINE_END "\r\n"
#define PAGE_404 "404.html"

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
static std::string Suffix2Desc(const std::string &suffix)
{
    static std::unordered_map<std::string, std::string> suffix_kv = {
        {".html", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".jpg", "application/x-jpg"},
        {".xml", "application/xml"},
    };

    auto iter = suffix_kv.find(suffix);
    if (iter != suffix_kv.end())
    {
        return iter->second;
    }

    return "text/html";
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
    bool cgi;                                               // 处理方式
    std::string suffix;                                     //资源后缀

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
    int src_fd;   //资源fd
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
        //读取正文 在 解析报头之后
    }
    void ParseHttpRequest() // 解析请求
    {
        ParseHttpRequestLine();   // 解析请求行
        ParseHttpRequestHeader(); // 解析请求报头
        RecvHttpRequestBody();    // 读取正文
    }
    void BulidHttpResponse() //构建响应
    {
        //判断请求方式
        //判断资源路径合法性
        //判断处理方式

        struct stat st;   //写网页挺重要
        int src_size = 0; //资源文件大小
        int l_found;      //资源后缀

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
                //带参 -- 切割URI
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
            _http_request.path = _http_request.uri;
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
            _http_request.path += PAGE_HOME;
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
                //b. 是目录 -- 访问目录下的默认网页
                _http_request.path += "/";
                _http_request.path += PAGE_HOME;
                stat(_http_request.path.c_str(), &st); // 更新st -- 资源发生变化
            }

            //判断资源是否是可执行文件 -- CGI机制
            //man 2 stat
            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            {
                //c. 可执行文件
                _http_request.cgi = true;
            }
            _http_responce.src_size = src_size; //正文大小
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
        //处理后缀
        l_found = _http_request.path.rfind(".");
        if (l_found == std::string::npos)
        {
            _http_request.suffix = ".html";
        }
        else
        {
            _http_request.suffix = _http_request.path.substr(l_found);
        }

        //处理方式 1.cgi 2. no cgi
        if (_http_request.cgi)
        {
            // post/get+带参，可执行文件
            code = ProcessCgi(); //执行目标程序，将结果放到responce_body
        }
        else
        {
            code = ProcessNonCgi(); //打开静态资源
        }

#ifdef DEBUG
        LOG(DEBUG, _http_request.method);
        LOG(DEBUG, _http_request.path);
        LOG(DEBUG, _http_request.version);
#endif

    END:

        BulidHttpResponseHelper(); //构建响应
    }
    void SendHttpResponse() //发送响应
    {
        send(_sock, _http_responce.responce_line.c_str(), _http_responce.responce_line.size(), 0); //状态行
        for (auto iter : _http_responce.responce_header)                                           //报头
        {
            send(_sock, iter.c_str(), iter.size(), 0);
        }
        send(_sock, _http_responce.blank.c_str(), _http_responce.blank.size(), 0); //空行
        if (_http_request.cgi)//cgi
        {
            auto &responce_body = _http_responce.responce_body;
            int size = 0;
            int  total = 0;
            const char *start = responce_body.c_str();
            while(total < responce_body.size() && (size = send(_sock, start + total, responce_body.size() - total, 0)) > 0)
            {
                total += size;
            } 
        }
        else//no cgi
        {
            sendfile(_sock, _http_responce.src_fd, nullptr, _http_responce.src_size); 
            close(_http_responce.src_fd);
        }
    }

private:
    void RecvHttpRequestLine()
    {
        //状态行
        Util::ReadLine(_sock, _http_request.request_line);
        _http_request.request_line.resize(_http_request.request_line.size() - 1); //去除 行结尾标志

#ifdef DEBUG
        LOG(DEBUG, _http_request.request_line);
#endif
    }
    void RecvHttpRequestHeader()
    {
        //请求报头
        std::string line;
        while ("\n" != line) // 循环读取
        {
            line.clear(); //清空
            Util::ReadLine(_sock, line);
            if ("\n" == line)
            {
                _http_request.blank = line;
                break;
            }

            line.resize(line.size() - 1); //去除行结尾标志 '\n'
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
                else if (s == 0) //对端关闭
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
        //POST后续 需要读取正文
        //而GET的正文会在URI中一同传递
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
    int ProcessNonCgi() //打开静态资源
    {
        //打开静态资源文件
        _http_responce.src_fd = open(_http_request.path.c_str(), O_RDONLY);
        if (_http_responce.src_fd >= 0)
        {
            return OK;
        }
        else
        {
            LOG(INFO, "scr_file open error!");
            return NOT_FOUND;
        }
    }
    int ProcessCgi()
    {
        LOG(INFO, "Cgi...");

        //1. 如何执行指定进程
        //使用进程替换，进行CGI机制
        //当前线程是httpserver线程下的子进程，若直接在该线程下替换，则整个进程都会发生进程替换，服务器就会终止
        //所以，需要先创建一个子进程，使子进程进行进程替换
        //2. 父子进程如何通信， 即子进程如何获取参数，父进程如何获取结果
        //匿名管道，两套
        //3. 子进程获取参数
        //匿名管道
        //环境变量
        //此时子进程获取参数的方法不止一个，所以还需要获取 如何获取参数
        //环境变量

        int code = OK;

        auto &query_string = _http_request.query_string;     // GET
        auto &body_text = _http_request.request_body;        // POST
        auto &method = _http_request.method;                 //请求方式
        auto &bin = _http_request.path;                      //exe路径
        auto &content_length = _http_request.content_length; //post正文长度
        auto &respnce_body = _http_responce.responce_body;

        std::string query_string_env; //GET传参时需要的环境变量
        std::string method_env;       //  请求方式环境变量
        std::string content_length_env;

        //站在父进程角度创建两个匿名管道
        int input[2];
        int output[2];
        if (pipe(input) < 0) //创建失败
        {
            LOG(ERROR, "pipe input error!");
            code = NOT_FOUND;
            return code;
        }
        if (pipe(output) < 0)
        {
            LOG(ERROR, "pipe output error!");
            code = NOT_FOUND;
            return code;
        }

        pid_t pid = fork();
        if (pid == 0) //子进程
        {
            //关闭不需要的权限
            close(input[0]);
            close(output[1]);

            //进程替换只会替换代码+数据，不会新建PCB，会保留原先的系统资源
            //但进程地址空间的数据，比如局部变量，全局变量，会消失
            //所以我们约定，替换后的进程，读取管道等价于读取标准输入0，写入管道，等价于写到标准输出
            //即替换之前，对子进程进行重定向
            // input[1]: 子进程写入， 子进程 写到1 --》 子进程写到 input[1];
            // output[0]：子进程读取，子进程 读取0 --》 子进程读取 output[0];
            dup2(input[1], 1); //本来fd1执行标准输出，现在指向input[1]
            dup2(output[0], 0);
            //之后的测试输出都需要更改cout->cerr

            //确定获取参数的方法
            method_env = "METHOD=";
            method_env += method;
            putenv((char *)method_env.c_str());
#ifdef DEBUG
            LOG(DEBUG, "Get Method, add method_env!"); //这里需要更改LOG：cout->cerr
#endif

            if (method == "GET")
            {
                query_string_env = "QUERY_STRING=";
                query_string_env += query_string;
                putenv((char *)query_string_env.c_str());

                LOG(INFO, "Get Method: GET, add query_string_env ..."); //这里需要更改LOG：cout->cerr
            }
            else if ("POST" == method)
            {
                content_length_env = "CONTENT_LENGTH=";
                content_length_env += std::to_string(content_length);
                putenv((char *)content_length_env.c_str());

                LOG(INFO, "Get Method: POST, add qcontent_length_env ..."); //这里需要更改LOG：cout->cerr
            }
            else
            {
                ;
            }

            execl(bin.c_str(), bin.c_str(), nullptr);

            //子进程不需要手动释放，结束进程自动释放

            LOG(ERROR, "execl error!");
            exit(1);
        }
        else if (pid < 0) //fork error
        {
            return NOT_FOUND;
        }
        else //parent
        {
            //关闭不需要的权限
            close(input[1]);
            close(output[0]);

            //传参
            if ("POST" == method) // POST 使用管道传给子进程
            {
                int size = body_text.size(); //总大小
                const char *start = body_text.c_str();
                int total = 0; //已写入的大小
                int cnt = 0;   //本次写入的大小
                while ((cnt = write(output[1], start + total, size - total)) > 0)
                {
                    total += cnt;
                }
            }
            else // GET 使用环境变量传给子进程
            {
                ; //在子进程{}设置
                //环境变量具有全局属性，则子进程是能够继承到父进程的环境变量的
                //且环境变量不受execp程序替换的影响
            }

            //读取cgi返回结果，构建正文
            char ch = 0;
            while (read(input[0], &ch, 1) > 0)
            {
                respnce_body.push_back(ch);
            }

            int status = 0;
            pid_t ret = waitpid(pid, &status, 0); //阻塞等
            if (ret == pid)
            {
                if (WIFEXITED(status)) //正常退出
                {
                    if (WEXITSTATUS(status) == 0) //且退出码为0
                    {
                        code = OK;
                    }
                    else
                    {
                        code = NOT_FOUND;
                    }
                }
                else
                {
                    code = NOT_FOUND;
                }
            }

            //释放资源
            close(input[0]);
            close(output[1]);
        }

        return code;
    }
    void BulidOkResponce()
    {
        // 构建响应报头
        std::string header_line;
        header_line = "Content-Type: "; //正文类型
        header_line += _http_request.suffix;
        header_line += LINE_END;
        _http_responce.responce_header.push_back(header_line);
        header_line = "Content-Length: "; //正文长度
        if (_http_request.cgi)
        {
            header_line += std::to_string(_http_responce.responce_body.size());
        }
        else
        {
            header_line += std::to_string(_http_responce.src_size);
        }
        header_line += LINE_END;
        _http_responce.responce_header.push_back(header_line);
    }
    void HandlerError(std::string page) //404错误处理
    {

        _http_request.cgi = false;

        //返回给用户对应的错误页面
        _http_responce.src_fd = open(PAGE_404, O_RDONLY);
        if (_http_responce.src_fd > 0)
        {
            struct stat st;
            stat(page.c_str(), &st);
            _http_responce.src_size = st.st_size;
            // 构建响应报头
            std::string header_line;
            header_line = "Content-Type: text/html"; //正文类型
            header_line += LINE_END;
            _http_responce.responce_header.push_back(header_line);
            header_line = "Content-Length: "; //正文长度
            header_line += std::to_string(_http_responce.src_size);
            header_line += LINE_END;
            _http_responce.responce_header.push_back(header_line);
        }
    }
    void BulidHttpResponseHelper() //错误处理
    {
        auto &code = _http_responce.status_code; //code

        //构建状态行
        auto &responce_line = _http_responce.responce_line;
        responce_line += HTTP_VERSION;
        responce_line += " ";
        responce_line += std::to_string(code);
        responce_line += " ";
        responce_line += Code2Desc(code);
        responce_line += LINE_END;

        //构建响应正文，可能有响应报头
        switch (code)
        {
        case OK:
            BulidOkResponce();
        case NOT_FOUND:
            HandlerError(PAGE_404);
            break;
        default:
            break;
        }
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

        std::cout << "get a new link ... : " << sock << std::endl;

#ifdef DEBUG
        char buffer[4096];
        recv(sock, buffer, sizeof(buffer), 0);
        std::cout << "----------------------begin--------------------------" << std::endl;
        std::cout << buffer << std::endl;
        std::cout << "----------------------end--------------------------\n"
                  << std::endl;
#else
        //一次HTTP通信
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

        close(sock);
        LOG(INFO, "Hander Request end.");
        return nullptr;
    }
};