# 线程池版本

## 图

### 流程图

![](%E5%9B%BE%E7%89%87/README/%E5%9F%BA%E7%A1%80%E6%B5%81%E7%A8%8B.drawio.png)

### UML类图

![](%E5%9B%BE%E7%89%87/README/UML.drawio.png)

### 一次Http处理

![](%E5%9B%BE%E7%89%87/README/%E4%B8%80%E6%AC%A1Http%E5%A4%84%E7%90%86.drawio.png)

## LOG日志管理

~~~C++
// Log.hpp
#pragma once

#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>

enum
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};
const char *const levalStr[] = {
    "DEBUG",
    "INFO",
    "WARING",
    "ERROR",
    "FATAL"};


// #define LOG(leval, message) Log(levalStr[leval], message, __FILE__, __LINE__)
#define LOG(leval, message) Log(#leval, message, __FILE__, __LINE__)

void Log(std::string leval, std::string message, std::string file_name, int line)
{
    std::cerr << "[" << leval << "]"
              << "[" << time(nullptr) << "]"
              << "[" << message << "]"
              << "[" << file_name << "___" << line << "]" << std::endl;
}
~~~

## ThreadPool线程池

线程可以处理中型数据请求，相较于给每一个请求都创建一个线程进行数据处理，线程池的优势是：减少了线程的创建与销毁，可以避免大量请求同时到来时，系统资源崩溃。

[线程池版本]([HttpWeb/HttpWeb_ThreadPool at main · tianen1573/HttpWeb (github.com)](https://github.com/tianen1573/HttpWeb/tree/main/HttpWeb_ThreadPool))

~~~C++
// ThreadPool.hpp
#pragma once

#include <iostream>
#include <queue>
#include <pthread.h>

#include "Task.hpp" // 任务类
#include "Log.hpp" // LOG类

#define NUM 6 // 最大线程数

// 可编写为类模板
class ThreadPool
{

private:
    ThreadPool(int num = NUM)
        :_num(num), _stop(false)
    {
        pthread_mutex_init(&_lock, nullptr);
        pthread_cond_init(&_cond, nullptr);
    }
    ThreadPool(const ThreadPool &tp) = delete;
    ThreadPool operator=(const ThreadPool &tp) = delete;

public:
    static ThreadPool* GetInstance()
    {
        pthread_mutex_t signal_mtx = PTHREAD_MUTEX_INITIALIZER;
        if(_signal_instance == nullptr)
        {
            pthread_mutex_lock(&signal_mtx);
            if(_signal_instance == nullptr)
            {
                _signal_instance = new ThreadPool();
                _signal_instance->InitThreadPool();
            }
            pthread_mutex_unlock(&signal_mtx);
        }

        return _signal_instance;
    }
    ~ThreadPool()
    {
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_cond);
        delete _signal_instance;
    }

public:
    bool InitThreadPool()
    {
        for(int i = 0; i < _num; ++ i)
        {
            pthread_t tid;
            if(pthread_create(&tid, nullptr, ThreadRoutine, this) != 0)
            {
                LOG(FATAL, "ThreadPool Init Error!");
                return false;
            }
            
        }
        LOG(INFO, "ThreadPool Create Success ...");
        return true;
    }
    void PushTask(const Task task)
    {
        Lock();
        _task_queue.push(task);
        UnLock();
        ThreadWakeup();
    }
    void PopTask(Task &task)
    {
        task = _task_queue.front();
        _task_queue.pop();
    }

    void ThreadWait()
    {
        pthread_cond_wait(&_cond, &_lock);
    }
    void ThreadWakeup()
    {
        pthread_cond_signal(&_cond);
    }
    void Lock()
    {
        pthread_mutex_lock(&_lock);
    }
    void UnLock()
    {
        pthread_mutex_unlock(&_lock);
    }
    bool IsStop()
    {
        return _stop;
    }
    bool TaskQueueEmpty()
    {
        return _task_queue.empty();
    }

    static void *ThreadRoutine(void *args)
    {
        // 必须是静态的， 因为非静态成员函数会有一个this指针形参， 无法作为回调函数
        ThreadPool *tp = (ThreadPool *)args;

        while(true)
        {
            Task t;
            tp->Lock();
            while(tp->TaskQueueEmpty()) //while保证健壮性，避免伪唤醒
            {
                tp->ThreadWait();
            }
            tp->PopTask(t);
            tp->UnLock();
            t.ProcessOn();
        }
    }

private:
    int _num; // 线程数量
    bool _stop; // 线程池运行状态
    std::queue<Task> _task_queue; // 任务队列
    pthread_mutex_t _lock; // 互斥锁
    pthread_cond_t _cond; // 条件变量 -- 同步队列

    static ThreadPool* _signal_instance;
};
ThreadPool* ThreadPool::_signal_instance;

~~~

### 线程池初始化

1. 确定线程池的线程数量
2. 确定线程的回调函数

~~~C++
bool InitThreadPool()
{
    for(int i = 0; i < _num; ++ i)
    {
        pthread_t tid;
        if(pthread_create(&tid, nullptr, ThreadRoutine, this) != 0)
        {
            LOG(FATAL, "ThreadPool Init Error!");
            return false;
        }

    }
    LOG(INFO, "ThreadPool Create Success ...");
    return true;
}
~~~

~~~C++
private:
    int _num; // 决定线程池里的线程数量
    bool _stop; // 线程池运行状态
~~~

### 生成者消费者模型

线程池的任务管理是一个简单的生产者消费者模型，我们的主线程即httpserver执行流，作为生产者角色，一直往临界区Push任务；而线程池里的线程(称任务线程)会循环Pop任务，执行任务的handler函数。

在这个过程中，我们需要互斥锁和条件变量，用来保护临界区资源和同步线程避免产生饥饿。

所以，我们需要的成员变量如下：

~~~C++
private:
    std::queue<Task> _task_queue; // 任务队列 -- 临界区
    pthread_mutex_t _lock; // 互斥锁 -- 消费者PopTask的互斥锁
    pthread_cond_t _cond; // 条件变量 -- 同步队列避免线程饥饿
~~~

互斥锁和同步队列管理函数如下：

~~~C++
public:
	void ThreadWait() // 线程等待 -- 任务线程调用
    {
        pthread_cond_wait(&_cond, &_lock);
    }
    void ThreadWakeup() // 唤醒线程 -- 主线程调用
    {
        pthread_cond_signal(&_cond);
    }
    void Lock() // 互斥锁加锁
    {
        pthread_mutex_lock(&_lock);
    }
    void UnLock() // 互斥锁解锁
    {
        pthread_mutex_unlock(&_lock);
    }
~~~

临界区资源相关函数如下：

~~~C++
public:
	void PushTask(const Task task)
    {
        Lock(); // 加锁
        _task_queue.push(task); // push
        UnLock(); // 解锁
        ThreadWakeup(); // 唤醒一个等待任务线程
    }
    void PopTask(Task &task)
    {
        // 调用PopTask，我们保证临界区存在资源，并且是在加锁条件下调用的
        // 所以，在PopTask函数里不需要线程安全控制
        task = _task_queue.front();
        _task_queue.pop();
    }
	bool TaskQueueEmpty()
    {
        // STL容器不是线程安全的
        return _task_queue.empty();
    }
~~~

### 线程回调函数

性质：

1. 返回值和形参列表是**确定的**，`void*`和`void*`，并且是**可重入的**，有时还需要是**线程安全的**。
2. 决定线程在被创建后，执行流的工作内容，也是线程池类最关键的一部分。
3. 线程回调函数可在**线程池对象构造时传参**获取，也可作为**静态成员函数**(非静态成员函数，有隐藏的this形参)。

实现：

~~~C++
static void *ThreadRoutine(void *args)
{
    // 必须是静态的， 因为非静态成员函数会有一个this指针形参， 无法作为回调函数
    // 传参获取的线程回调函数并不需要是static
    ThreadPool *tp = (ThreadPool *)args;

    while(true) // 持久化运行
    {
        Task t;
        tp->Lock();
        while(tp->TaskQueueEmpty()) // while保证健壮性，避免伪唤醒
        {
            tp->ThreadWait();
        }
        tp->PopTask(t); // 保证PopTask的调用是加锁的
        tp->UnLock();
        t.ProcessOn(); // 和Task类的实现有关
    }
}
~~~

### 单例模式

~~~C++
static ThreadPool* GetInstance()
{
    pthread_mutex_t signal_mtx = PTHREAD_MUTEX_INITIALIZER;
    if(_signal_instance == nullptr)
    {
        pthread_mutex_lock(&signal_mtx);
        if(_signal_instance == nullptr)
        {
            _signal_instance = new ThreadPool();
            _signal_instance->InitThreadPool();
        }
        pthread_mutex_unlock(&signal_mtx);
    }

    return _signal_instance;
}
~~~

## Task类

Task类主要有两个需要注意事项：

1. 成员变量：主要是给Task类的回调函数传参
2. 回调函数：回调函数用来执行实际任务，本次我们使用的是 **仿函数对象成员变量**

~~~C++
// Task.hpp
#pragma once

#include <unistd.h>
#include "Protocol.hpp"

class Task
{
public:
    Task()
    {
    }
    Task(int sock)
        : _sock(sock)
    {
    }
    ~Task()
    {
        // 此时不能关闭sock，因为我们的Task对象是 在while循环的栈里 创建出来的，出作用域会被释放
        // 我们实现的逻辑是，无论是传址还是传参，都会在一次循环结束后析构，会关闭sock，则后续读取请求时，读取会失败
        // sock最终会被传到Endpoint类，在该对象内，Send函数结束后，delete时，关闭该sock
        // 即使new出来的Task对象， 我们也应该把sock放到EndPoint对象里关闭，保证唯一性
        // close(_sock);
    }

public:
    void ProcessOn()
    {
        _handler(_sock);
    }

private:
    int _sock;
    CallBack _handler; // 回调函数对象
};
~~~

## TcpServer

Tcp编程流程：**创建监听套接字，绑定套接字，监听套接字，获取链接**。

该类包含了前三个过程，获取链接我们交给HttpServer循环获取。

同样的，我们也将其实现为单例模式。

~~~C++
// TcpServer.hpp
#pragma once

#include <iostream>
#include <cstdlib>
#include <cassert>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <cstring>

#include "Log.hpp"

#define BACKLOG 5

class TcpServer
{
private:

    //私有化构造函数，拷贝构造，赋值构造
    TcpServer(uint16_t port)
        : _port(port)
        , _listen_sock(-1)
    {
    }
    TcpServer(const TcpServer & s) = delete;
    TcpServer operator=(const TcpServer & s) = delete;

public:
    static TcpServer* GetInstance(int port)//单例模式
    {
        static pthread_mutex_t mtx_svr = PTHREAD_MUTEX_INITIALIZER;// 静态锁可以直接初始化
        if(svr == nullptr)
        {
            pthread_mutex_lock(&mtx_svr);

            if(nullptr == svr)
            {
                svr = new TcpServer(port);
                svr->InitServer();
            }

            pthread_mutex_unlock(&mtx_svr);
        }

        return svr;
    }
    ~TcpServer()
    {
        if(_listen_sock >= 0)
        {
            close(_listen_sock);
            delete svr;
        }
    }

public:
    void InitServer()
    {
        Socket(); // 创建监听套接字
        Bind(); // 绑定套接字
        Listen(); // 监听
        LOG(INFO, "init ... success.");
    }
    void Socket()
    {
        _listen_sock = socket(AF_INET, SOCK_STREAM, 0);//创建套接字
        if(_listen_sock < 0)
        {
            LOG(FATAL, "create socket error!");
            exit(1);
        }

        //在TCP连接中，先退出的一方会维持一段时间的TIME_WAIT状态，该状态下的套接字不会立即释放它所申请的资源
        //只有过了2MSL时间，才会释放资源，比如释放端口号，fd等，则这段时间内，一些系统资源是不能使用的
        //socket地址复用，不等待TIME_WAIT状态退出，强制忽略该状态，使用相应的系统资源
        //则_listen_sock，在后续绑定时，不再需要等待TIME_WAIT状态，本质是允许一个端口号绑定多个进程
        int opt = 1;
        setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
        LOG(INFO, "create socket ... success.");
    }
    void Bind()
    {
        struct sockaddr_in local;//通信结构体
        memset(&local, 0, sizeof local);
        local.sin_family = AF_INET;//协议类型
        local.sin_port = htons(_port);//端口号
        local.sin_addr.s_addr = INADDR_ANY;//IP
        if(bind(_listen_sock, (struct sockaddr*)&local, sizeof local) < 0)
        {
            LOG(FATAL, "bind error!");
            exit(2);
        }
        LOG(INFO, "bind socket ... success.");
    }
    void Listen()
    {
        if(listen(_listen_sock, BACKLOG) < 0)
        {
            LOG(FATAL, "listen socket error!");
            exit(3);
        }
        LOG(INFO, "listen socket ... success.");
    }

    int GetSock()
    {
        return _listen_sock;
    }

private:
    int _port;
    int _listen_sock;
    // string _ip; // 服务端不需要显示绑定IP， 绑定INADDR_ANY，可接受所有网卡的请求，并且云服务器不能绑定公网IP
    // bool _stop; // 如果TcpServer初始化失败了，可以交给HttpServer进行处理(重新初始化 或者 重启服务器)
 
    static TcpServer* svr;// 懒汉单例模式
};
TcpServer* TcpServer::svr = nullptr;

~~~

## HttpServer

HttpServer对象循环获取http请求即获取tcp链接，并把该链接以Task对象的方式交给任务线程。

则主线程(HttpServer执行流)获取链接生成任务，任务线程拿去任务解决任务。

~~~C++
// HttpServer.hpp
#pragma once

#include <iostream>

#include <signal.h>
#include <pthread.h>

#include "TcpServer.hpp"
#include "Protocol.hpp"
#include "Log.hpp"
#include "Task.hpp"
#include "ThreadPool.hpp"

#define PORT 8081

class HttpServer
{

public:
    HttpServer(int port = PORT)
        : _port(port), _stop(false)
    {
    }
    ~HttpServer()
    {
    }

public:
    void InitServer()
    {
        signal(SIGPIPE, SIG_IGN); // 忽略sigpipe信号， 避免http进程终止
        _signal_tcp = TcpServer::GetInstance(_port);
        _signal_tp = ThreadPool::GetInstance();
    }
    void Loop()
    {
        LOG(INFO, "Loop begin ...");
        while (_stop == false)
        {
            struct sockaddr_in peer;
            socklen_t len = sizeof peer;// 发送方数据
            int sock = accept(_signal_tcp->GetSock(), (struct sockaddr*)&peer, &len);
            if(sock < 0)
            {
                continue;
            }
            LOG(INFO, "Get a new link ...,and sock: " + std::to_string(sock));
            Task task(sock);
            _signal_tp->PushTask(task);
        }
    }

private:
    int _port;
    bool _stop;
    
    TcpServer *_signal_tcp;
    ThreadPool *_signal_tp;
    // TcpServer *_tcp_server;
    // ThreadPool * _thread_pool;
};
~~~

## 小结

此时我们实现了基本流程的上半部分，即如何获取Http请求。

![image-20230709215556341](%E5%9B%BE%E7%89%87/README/image-20230709215556341.png)

接下来就是，如何解决Http请求，并返回Http响应。

![image-20230709215722793](%E5%9B%BE%E7%89%87/README/image-20230709215722793.png)

## CallBack

CallBack对象作为Task对象的仿函数，以执行指定任务，CallBack没有设计的很复杂，只是宏观上执行四个函数，完成一次 解析Http请求 + 返回Http响应。

~~~C++
class CallBack
{
public:
    CallBack()
    {
    }
    void operator()(int sock)
    {
        HandlerRequest(sock);
    }
    ~CallBack()
    {
    }

public:
    void HandlerRequest(int sock)
    {
        LOG(INFO, "Hander Request begin ...");

        // 一次HTTP通信
        _ep= new EndPoint(sock);
        _ep->RcvHttpRequest();
        if (_ep->IsStop() == false) // 只有成功读取Http请求的全部内容，才执行剩下任务
        {
            LOG(INFO, "Recv No Error, Begin Prase ...");
            _ep->ParseHttpRequest(); 

            if (_ep->IsStop() == false) // 只有成功读取Http请求的全部内容，才执行剩下任务
            {
                LOG(INFO, "Prase No Error, Begin Bulid and Send ...");
                _ep->BulidHttpResponse();
                _ep->SendHttpResponse();
            }
            else
            {
                LOG(WARNING, "Prase Error, Stop Bulid and Send!");
            }
        }
        else
        {
            LOG(WARNING, "Recv Error, Stop Prase!");
        }
        delete _ep;
        close(sock);
        LOG(INFO, "Hander Request end ...");
    }

private:
    EndPoint *_ep;
};
~~~

## Http协议格式

balabala

## HttpRequest

保存Http请求的相关数据， 和解析后的数据.

~~~C++
//Http请求
class HttpRequest
{
public:
    // 请求
    std::string request_line;                //状态行
    std::vector<std::string> request_header; //报头
    std::string blank;                       // 空行
    std::string request_body;                // POST正文
    std::string query_string;                // GET正文

    // 解析结果
    std::string method;                                     // 请求方法
    std::string uri;                                        // 请求资源 path?args
    std::string version;                                    // HTTP协议版本
    std::unordered_map<std::string, std::string> header_kv; // 报头kv
    int content_length;                                     // 正文长度
    std::string path;                                       // 资源路径 ： 处理后
    bool cgi;                                               // 处理方式
    std::string suffix;                                     // 资源后缀

public:
    HttpRequest()
        : content_length(0), cgi(false)
    {
    }
    ~HttpRequest()
    {
    }
};
~~~

## HttpResponse

保存Http响应的相关数据。

~~~C++
class HttpResponse
{
public:
    std::string responce_line;                // 状态行
    std::vector<std::string> responce_header; // 报头
    std::string blank;                        // 空行
    std::string responce_body;                // 正文

    int status_code; // 状态码
    int src_fd;      // 静态资源fd
    int src_size;    // 静态资源大小
public:
    HttpResponse()
        : blank(LINE_END), status_code(0), src_fd(-1), src_size(0)
    {
    }
    ~HttpResponse()
    {
    }
};
~~~

## Util工具类

实现一些共用的函数。

~~~ C++
// Util.hpp
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
        //处理不同格式的 行结尾
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
            else // 对端关闭，recv失败
            {
                return 0;
                break;
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
~~~

## EndPoint

本次HttpWeb项目的核心类，包含**读取请求，解析请求，构建响应，发送响应**，四个核心功能。

~~~C++
class EndPoint
{
public:
    EndPoint(int sock)
        : _sock(sock), _stop(false)
    {
    }
    ~EndPoint()
    {
        close(_sock);
    }

public:
    void RcvHttpRequest() //读取请求
    {
        // 读取请求行
        // 读取请求报头
        // 读取正文 ：解析报头，判断是否需要读取正文

        // 逻辑短路
        (RecvHttpRequestLine() || RecvHttpRequestHeader()); // _stop为真 -- 读取出错
    }
    void ParseHttpRequest() // 解析请求
    {
        ParseHttpRequestLine();   // 解析请求行
        ParseHttpRequestHeader(); // 解析请求报头
        RecvHttpRequestBody();    // 读取正文
    }
    void BulidHttpResponse() //构建响应
    {
        // 1. 判断请求方式
        // 2. 处理资源路径
        // 3. 处理请求 -- cgi/no cgi
        // 4. 构建响应

        struct stat st;                          // 读取文件信息
        int l_found;                             // 资源后缀下标
        auto &code = _http_responce.status_code; // 状态码
        code = OK;                               // 默认为OK

        // 1. 判断请求方式
        if ("GET" != _http_request.method && "POST" != _http_request.method) // 非法请求方式
        {
            LOG(WARNING, "method is not right!");
            code = BAD_REQUEST;
            goto END;
        }
        if ("GET" == _http_request.method)
        {
            ssize_t pos = _http_request.uri.find('?');
            if (pos != std::string::npos) // 带参 -- 切割URI
            {
                Util::CutString(_http_request.uri, _http_request.path, _http_request.query_string, "?");
                _http_request.cgi = true; // cgi机制处理
            }
            else // 不带参
            {
                _http_request.path = _http_request.uri;
            }
#ifdef DEBUG // 判断切割是否正确
            LOG(DEBUG, "GET, and ...");
            LOG(DEBUG, _http_request.uri);
            LOG(DEBUG, _http_request.path);
            LOG(DEBUG, _http_request.query_string);
#endif
        }
        else if ("POST" == _http_request.method)
        {
            _http_request.path = _http_request.uri;
            _http_request.cgi = true;
#ifdef DEBUG // 判断切割是否正确
            LOG(DEBUG, "POST, and ...");
            LOG(DEBUG, _http_request.uri);
            LOG(DEBUG, _http_request.path);
#endif
        }
        else
        {
            ;
        }

        // 2. 判断路径合法性
        //如何保证path的合法性？ 1. / 代表什么 2. 路径对应的资源 是否在服务器上存在
        //1. / 不一定代表根目录， 一般情况下 会指明web根目录：wwwroot，并且还需要默认首页，则 / ->> wwwroot/ ，/a/b/c ->> webroot/a/b/c
        //2. 确定资源是否存在， 确认wwwroot目录下， 某个文件是否存在， 使用系统调用stat
        //3. 处理后缀 -- no cgi

        // 2.1 添加路径前缀
        _http_request.path = WEB_ROOT + _http_request.path; // 路径添加前缀
        if ("wwwroot/" == _http_request.path)               // 判断是否为 wwwroot/
        {
            _http_request.path += PAGE_HOME;
        }
        // 2.2 判断资源是否合法 -- man 2 stat
        // struct stat st;  // -- 写在goto前
        // int src_size = 0; // -- 写在goto前
        if (stat(_http_request.path.c_str(), &st) == 0) // 资源存在
        {
            // 访问的路径不一定是一个确定的文件，可能是一个目录，所以一般情况下，每个目录下都会构建一个默认访问文件，访问该目录时，即访问这个文件

            if (S_ISDIR(st.st_mode)) // 判断路径是否是目录， 是目录 -- 访问目录下的默认网页
            {
                _http_request.path += "/";
                _http_request.path += PAGE_HOME;
                stat(_http_request.path.c_str(), &st); // 更新st -- 资源发生变化
            }
            if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) // 判断资源是否是可执行文件 -- CGI机制
            {
                _http_request.cgi = true;
            }
            _http_responce.src_size = st.st_size; // 正文大小 -- 静态资源的大小， cgi程序并不使用该变量
#ifdef DEBUG
            LOG(DEBUG, _http_request.path);
            if (_http_request.cgi == false)
            {
                LOG(DEBUG, std::to_string(_http_responce.src_size));
            }
#endif
        }
        else // 资源不存在
        {
            LOG(WARNING, _http_request.path + " Not Found!");
            code = NOT_FOUND;
            goto END;
        }
        // 2.3 处理后缀
        l_found = _http_request.path.rfind(".");
        if (l_found == std::string::npos)
        {
            _http_request.suffix = ".html"; // 统一默认为html
        }
        else
        {
            _http_request.suffix = _http_request.path.substr(l_found);
        }
#ifdef DEBUG
        LOG(DEBUG, _http_request.suffix);
#endif
        //3. 处理方式 1.cgi 2. no cgi
        if (_http_request.cgi) // post/get+带参，可执行文件
        {
            code = ProcessCgi(); //执行目标程序，将结果放到responce_body
        }
        else
        {
            code = ProcessNonCgi(); //打开静态资源
        }

    END:                           // 根据状态码统一处理响应
        BuildHttpResponseHelper(); // 构建响应
    }
    void SendHttpResponse()// 发送响应
    {
        // 发送时，如果对方不读了，会产生sigpipe信号，使进程终止
        // 所以，在httpserver对象初始化时，需要忽略掉sigpipe信号，避免http进程被终止

#ifdef DEBUG
        LOG(DEBUG, std::to_string(_http_responce.status_code));
        if (_http_request.cgi)
        {
            LOG(DEBUG, "body_size: " + std::to_string(_http_responce.responce_body.size()));
        }
        else
        {
            LOG(DEBUG, "src_fd: " + std::to_string(_http_responce.src_fd));
        }
#endif

        send(_sock, _http_responce.responce_line.c_str(), _http_responce.responce_line.size(), 0); // 状态行
        for (auto iter : _http_responce.responce_header)                                           // 报头
        {
            send(_sock, iter.c_str(), iter.size(), 0);
        }
        send(_sock, _http_responce.blank.c_str(), _http_responce.blank.size(), 0); // 空行
        if (_http_request.cgi)                                                     // cgi 发送正文
        {
            auto &responce_body = _http_responce.responce_body;
            int size = 0;
            int total = 0;
            const char *start = responce_body.c_str();
            while (total < responce_body.size() && (size = send(_sock, start + total, responce_body.size() - total, 0)) > 0)
            {
                total += size;
            }
        }
        else // no cgi 发送正文
        {
            sendfile(_sock, _http_responce.src_fd, nullptr, _http_responce.src_size);
            close(_http_responce.src_fd);
        }
    }

private:
    bool RecvHttpRequestLine() // 状态行
    {
        if (Util::ReadLine(_sock, _http_request.request_line) > 0)
        {
            _http_request.request_line.resize(_http_request.request_line.size() - 1); //去除 行结尾标志
#ifdef DEBUG
            LOG(DEBUG, _http_request.request_line);
#endif
        }
        else
        {
            _stop = true;
        }
        return _stop;
    }
    bool RecvHttpRequestHeader() // 请求报头
    {
        std::string line;
        while ("\n" != line) // 循环读取
        {
            line.clear(); // 清空
            if (Util::ReadLine(_sock, line) <= 0)
            {
                _stop = true;
                break;
            }
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
        return _stop;
    }
    bool RecvHttpRequestBody() // post 正文
    {
        if (IsNeedRecvHttpRequestBody())
        {
#ifdef DEBUG
            LOG(DEBUG, "IsNeedRecvHttpRequestBody is yes...");
#endif
            int content_length = _http_request.content_length;
            auto &body = _http_request.request_body;
#ifdef DEBUG
            LOG(DEBUG, "Content-Length: " + std::to_string(content_length));
#endif
            char ch = 0;
            while (content_length)
            {
                ssize_t s = recv(_sock, &ch, 1, 0);
                if (s > 0)
                {
                    body.push_back(ch);
                    --content_length;
                }
                else // 对端关闭或出错
                {
                    LOG(ERROR, "recv error!");
                    _stop = true;
                    break;
                }
            }
#ifdef DEBUG
            LOG(DEBUG, "Request_Body: " + body);
#endif
        }
        return _stop;
    }
    void ParseHttpRequestLine() // 解析状态行
    {
        auto &line = _http_request.request_line;
        std::stringstream ss(line); // 流提取
        ss >> _http_request.method >> _http_request.uri >> _http_request.version;
        //把method转换成大写格式， 避免post/get出错 -- transform
        std::transform(_http_request.method.begin(), _http_request.method.end(), _http_request.method.begin(), ::toupper); //转大写
#ifdef DEBUG
        LOG(DEBUG, _http_request.method);
        LOG(DEBUG, _http_request.uri);
        LOG(DEBUG, _http_request.version);
#endif
    }
    void ParseHttpRequestHeader() // 解析请求报头
    {
        std::string key, value;
        for (auto &str : _http_request.request_header)
        {
            if (Util::CutString(str, key, value, SEP))
            {
                _http_request.header_kv.insert({key, value});
            }
        }
    }
    bool IsNeedRecvHttpRequestBody() // 判断是否需要读取 post正文
    {
        // POST后续 需要读取正文， 而GET的正文会在URI中一同传递
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
    
    int ProcessNonCgi() // No cgi ： 打开静态资源
    {
        LOG(INFO, "No Cgi...");
        _http_responce.src_fd = open(_http_request.path.c_str(), O_RDONLY); // 只读
        if (_http_responce.src_fd >= 0)
        {
#ifdef DEBUG
            LOG(DEBGU, _http_request.path + " is open...");
#endif
            struct stat st;
            stat(_http_request.path.c_str(), &st);
            _http_responce.src_size = st.st_size;
            return OK;
        }
        else
        {
            LOG(WARNING, "scr_file open error!");
            return NOT_FOUND;
        }
    }
    int ProcessCgi() // cgi ： 执行指定程序
    {
        LOG(INFO, "Cgi...");
        // 1. 如何执行指定进程
        // 使用进程替换，进行CGI机制
        // 当前线程是httpserver线程下的子进程，若直接在该线程下替换，则整个进程都会发生进程替换，服务器就会终止
        // 所以，需要先创建一个子进程，使子进程进行进程替换
        // 2. 父子进程如何通信， 即子进程如何获取参数，父进程如何获取结果
        // 匿名管道，两套
        // 3. 子进程获取参数 -- a.匿名管道 b.环境变量
        // 此时子进程获取参数的方法不止一个，所以还需要获取 如何获取参数？ --> 环境变量

        int code = OK;
        auto &query_string = _http_request.query_string;     // GET正文
        auto &body_text = _http_request.request_body;        // POST正文
        auto &method = _http_request.method;                 // 请求方式
        auto &bin = _http_request.path;                      // exe路径
        auto &content_length = _http_request.content_length; // post正文长度
        auto &responce_body = _http_responce.responce_body;  // 响应正文

        std::string query_string_env;   // GET传参时需要的环境变量
        std::string method_env;         //  请求方式环境变量
        std::string content_length_env; // Post传参时需要的环境变量

        // 1. 站在父进程角度创建两个匿名管道
        int input[2];
        int output[2];
        if (pipe(input) < 0) // 创建失败
        {
            LOG(ERROR, "pipe input error!");
            code = SERVER_ERROR;
            return code;
        }
        if (pipe(output) < 0) // 创建失败
        {
            // 此时input是成功的，需要关闭
            close(input[0]);
            close(input[1]);

            LOG(ERROR, "pipe output error!");
            code = SERVER_ERROR;
            return code;
        }

        // 2. 创建子进程
        pid_t pid = fork();
        if (pid == 0) // 子进程
        {
            // 关闭不需要的权限
            close(input[0]);
            close(output[1]);

            // 进程替换只会替换代码+数据，不会新建PCB，会保留原先的系统资源
            // 但进程地址空间的数据，比如局部变量，全局变量，会消失
            // 所以我们约定，替换后的进程，读取管道等价于读取标准输入0，写入管道，等价于写到标准输出
            // 即替换之前，对子进程进行重定向
            // input[1]: 子进程写入， 子进程 写到1 --》 子进程写到 input[1];
            // output[0]：子进程读取，子进程 读取0 --》 子进程读取 output[0];
            dup2(input[1], 1);  // 本来fd1指向标准输出，现在指向input[1]
            dup2(output[0], 0); // 同理
            // 之后的测试输出都需要更改cout->cerr

            // 3. 确定获取参数的方法
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
#ifdef DEBUG
                LOG(DEBUG, "Get Method: GET, add query_string_env ..."); //这里需要更改LOG：cout->cerr
#endif
            }
            else if ("POST" == method)
            {
                content_length_env = "CONTENT_LENGTH=";
                content_length_env += std::to_string(content_length);
                putenv((char *)content_length_env.c_str());
#ifdef DEBUG
                LOG(DEBUG, "Get Method: POST, add qcontent_length_env ..."); //这里需要更改LOG：cout->cerr
#endif
            }
            else
            {
                ;
            }

            execl(bin.c_str(), bin.c_str(), nullptr); // 此时bin已经能够获取参数

            // 子进程不需要手动释放fd，结束进程后自动释放

            LOG(ERROR, "execl error!");
            exit(1);
        }
        else if (pid < 0) // fork error
        {
            LOG(WARNING, "fork error!");
            return SERVER_ERROR;
        }
        else // parent
        {
            // 关闭不需要的权限
            close(input[1]);
            close(output[0]);

            // 3. 传参
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
                ; // 在子进程{}设置
                // 环境变量具有全局属性，则子进程是能够继承到父进程的环境变量的
                // 且环境变量不受execp程序替换的影响
            }

            // 4. 读取cgi返回结果，构建正文
            char ch = 0;
            while (read(input[0], &ch, 1) > 0)
            {
                responce_body.push_back(ch);
            }
            int status = 0;
            pid_t ret = waitpid(pid, &status, 0); // 阻塞等
            if (ret == pid)
            {
                if (WIFEXITED(status)) // 正常退出
                {
                    if (WEXITSTATUS(status) == 0) // 且退出码为0
                    {
                        code = OK;
                    }
                    else
                    {
                        code = BAD_REQUEST;
                    }
                }
                else
                {
                    code = SERVER_ERROR;
                }
            }

            // 释放资源
            close(input[0]);
            close(output[1]);
        }
        return code;
    }
    
    void BuildOkResponce()
    {
#ifdef DEBUG
        LOG(DEBUG, "BulidOkResponce");
#endif
        // 构建响应报头
        std::string header_line;
        header_line = "Content-Type: "; //正文类型
        header_line += Suffix2Desc(_http_request.suffix);
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
    void HandlerError(std::string page) // 错误处理
    {
#ifdef DEBUG
        LOG(DEBUG, "HandlerError");
#endif
        // 返回给用户对应的错误页面
        // 此时静态资源发生改变， 处理方式发生改变
        // 所以需要更改很多成员变量
        _http_request.cgi = false;                            // 此时处理方式强制为no cgi
        _http_request.suffix = ".html";                       // 静态资源强制为 html
        _http_responce.src_fd = open(page.c_str(), O_RDONLY); // 打开对应错误处理文件
        if (_http_responce.src_fd >= 0)
        {
            struct stat st;
            stat(page.c_str(), &st);
            _http_responce.src_size = st.st_size; // 更新资源大小
            // 构建响应报头
            std::string header_line;
            header_line = "Content-Type: text/html"; // 正文类型
            header_line += LINE_END;
            _http_responce.responce_header.push_back(header_line);
            header_line = "Content-Length: "; // 正文长度
            header_line += std::to_string(_http_responce.src_size);
            header_line += LINE_END;
            _http_responce.responce_header.push_back(header_line);
        }
        else
        {
            LOG(WARNING, "open error!");
        }
    }
    void BuildHttpResponseHelper() // 构建处理
    {
        auto &code = _http_responce.status_code; // code
        // 构建状态行
        auto &responce_line = _http_responce.responce_line;
        responce_line += HTTP_VERSION;
        responce_line += " ";
        responce_line += std::to_string(code);
        responce_line += " ";
        responce_line += Code2Desc(code);
        responce_line += LINE_END;

        // 构建响应正文，可能有响应报头
        switch (code)
        {
        case OK:
            BuildOkResponce();
            break;
        case BAD_REQUEST:
            HandlerError(PAGE_400);
            break;
        case NOT_FOUND:
            HandlerError(PAGE_404);
            break;
        case SERVER_ERROR:
            HandlerError(PAGE_500);
            break;
        default:
            break;
        }
    }

public:
    bool IsStop()
    {
        return _stop;
    }

private:
    int _sock;
    HttpRequest _http_request;
    HttpResponse _http_responce;

    bool _stop;
};
~~~

### 读取请求

读取请求分为：读取状态行，读取报头，而读取正文需要在解析状态行，解析报头后，才能确定读取正文需要的参数。

所以，在读取请求我们仅读取状态行和读取报头，读取正文我们放在解析后。

~~~C++
void RcvHttpRequest() //读取请求
{
    // 读取请求行
    // 读取请求报头
    // 读取正文 ：解析报头，判断是否需要读取正文

    // 逻辑短路
    (RecvHttpRequestLine() || RecvHttpRequestHeader()); // _stop为真 -- 读取出错
}
~~~

真正读取之前，我们需要了解HTTP协议格式，才能知道读取时，如何判断差错和结束。

详情看Until部分，这里我们直接使用。

#### 读取状态行

~~~C++
bool RecvHttpRequestLine() // 状态行
{
    if (Util::ReadLine(_sock, _http_request.request_line) > 0)
    {
        _http_request.request_line.resize(_http_request.request_line.size() - 1); //去除 行结尾标志
        #ifdef DEBUG
        LOG(DEBUG, _http_request.request_line);
        #endif
    }
    else
    {
        _stop = true;
    }
    return _stop;
}
~~~

#### 读取报头

~~~C++
bool RecvHttpRequestHeader() // 请求报头
{
    std::string line;
    while ("\n" != line) // 循环读取
    {
        line.clear(); // 清空
        if (Util::ReadLine(_sock, line) <= 0)
        {
            _stop = true;
            break;
        }
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
    return _stop;
}
~~~

### 解析请求

解析请求主要是：解析状态行，解析报头。

解析状态行：确定请求方式，请求资源，协议版本。

解析报头：把报头转换成kv模式，便于后续使用。

根据解析结果，确定是否需要读取正文。

~~~C++
void ParseHttpRequest() // 解析请求
{
    ParseHttpRequestLine();   // 解析请求行
    ParseHttpRequestHeader(); // 解析请求报头
    RecvHttpRequestBody();    // 读取正文
}
~~~

#### 解析状态行

请求状态行由三部分构成：请求方式，URI，协议版本。

我们使用流提取进行转换。

~~~C++
void ParseHttpRequestLine() // 解析状态行
{
    auto &line = _http_request.request_line;
    std::stringstream ss(line); // 流提取
    ss >> _http_request.method >> _http_request.uri >> _http_request.version;
    //把method转换成大写格式， 避免post/get出错 -- transform
    std::transform(_http_request.method.begin(), _http_request.method.end(), _http_request.method.begin(), ::toupper); //转大写
    #ifdef DEBUG
    LOG(DEBUG, _http_request.method);
    LOG(DEBUG, _http_request.uri);
    LOG(DEBUG, _http_request.version);
    #endif
}
~~~

#### 解析请求报头

把string切割成kv模型，供后续使用。

~~~C++
void ParseHttpRequestHeader() // 解析请求报头
{
    std::string key, value;
    for (auto &str : _http_request.request_header)
    {
        if (Util::CutString(str, key, value, SEP))
        {
            _http_request.header_kv.insert({key, value});
        }
    }
}
~~~

#### 读取正文

根据Http协议我们可以知道，http协议是可以带参的，并且当请求方式是post时，参数是以正文的形式传输的。

所以可以根据 POST+报头Content-Length，来判断是否需要读，读多少个。

~~~C++
bool IsNeedRecvHttpRequestBody() // 判断是否需要读取 post正文
{
    // POST后续 需要读取正文， 而GET的正文会在URI中一同传递
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
~~~

~~~C++
bool RecvHttpRequestBody() // post 正文
{
    if (IsNeedRecvHttpRequestBody())
    {
        #ifdef DEBUG
        LOG(DEBUG, "IsNeedRecvHttpRequestBody is yes...");
        #endif
        int content_length = _http_request.content_length;
        auto &body = _http_request.request_body;
        #ifdef DEBUG
        LOG(DEBUG, "Content-Length: " + std::to_string(content_length));
        #endif
        char ch = 0;
        while (content_length)
        {
            ssize_t s = recv(_sock, &ch, 1, 0);
            if (s > 0)
            {
                body.push_back(ch);
                --content_length;
            }
            else // 对端关闭或出错
            {
                LOG(ERROR, "recv error!");
                _stop = true;
                break;
            }
        }
        #ifdef DEBUG
        LOG(DEBUG, "Request_Body: " + body);
        #endif
    }
    return _stop;
}
~~~

### CGI机制

balabala

### stat

### 构建响应

构建响应是Http处理中最核心的部分，它需要根据解析后的Http请求，进行一系列处理，最后构建响应。在该函数内还要完善错误处理机制，因为http请求不一定是合法的，而且在整个处理过程充斥着大量的函数和系统调用，这些会出错的地方我们都需要考虑，并进行错误处理。

构建响应可以笼统地四步走：

1. 判断请求方式
2. 处理资源路径
3. 处理请求 -- cgi/no cgi
4. 构建响应

#### 判断请求方式

HTTP1.0最常见地两种请求方式，GET和POST，GET是URL传参，POST是正文传参。根据不同地请求方式我们要进行不同地处理。

GET：首先需要判断URI是否带参，即URI中是否含有'&'字符，如果含有'&'字符，我们就需要将URI切割成path+query_string，前者是实际的资源路径，后者是参数。

POST：POST的参数是在正文部分，我们已经读取过了，这里就不需要处理了。

在该部分我们还需要判断本次请求的处理方式：cgi / non-cgi，我们简单处理，只要带参一定是CGI，只要是POST一定是CGI。

#### 处理资源路径

有时候会发现，我们请求的资源和得到的资源有时候是不一样的，比如我请求的是`https://sports.qq.com/nba/`，但服务器返回的是`https://sports.qq.com/nba/index.shtml`。

虽然现在不知道怎么做到的，但我们也能够窥探到一点，即服务器一定将我们的请求的资源路径进行了处理，转换成了一种合乎服务器规则的新的资源路径，服务器的规则大同小异(应该？)。

如何判断资源路径合法性？我们至少需要搞清楚两个问题：一是域名后跟着的`/`代表什么，二是路径对应的资源是否存在，即服务器上是否存在这个路径，并且这个路径需要指向一个文件。

首先， `/`不一定代表根目录， 一般情况下 `/`是web根目录：`wwwroot`，则 `/` ->> `wwwroot/` ，`/a/b/c` ->> `webroot/a/b/c`。

其次，确定资源是否存在， 即确认`wwwroot`目录下， 某个文件是否存在， 我们使用系统调用stat(前面已介绍)。

为了后续实现，在这个部分我们还判断了：本次请求的处理方式，处理后缀等。

#### 处理方式

Cgi：简单来说，将客户端传输的参数进行处理后，返回一个结果。

NonCgi：返回指定的静态资源。

##### CGI

~~~C++
int ProcessCgi() // cgi ： 执行指定程序
{
    LOG(INFO, "Cgi...");
    // 1. 如何执行指定进程
    // 使用进程替换，进行CGI机制
    // 当前线程是httpserver线程下的子线程，若直接在该线程下替换，则整个进程都会发生进程替换，服务器就会终止
    // 所以，需要先创建一个子进程，使子进程进行进程替换
    // 2. 父子进程如何通信， 即子进程如何获取参数，父进程如何获取结果
    // a. 匿名管道，两套 b. 环境变量
    // 3. 子进程获取参数 -- a.匿名管道 b.环境变量
    // 此时子进程获取参数的方法不止一个，所以还需要获取 如何获取参数？ --> 环境变量

    int code = OK;
    auto &query_string = _http_request.query_string;     // GET正文
    auto &body_text = _http_request.request_body;        // POST正文
    auto &method = _http_request.method;                 // 请求方式
    auto &bin = _http_request.path;                      // exe路径
    auto &content_length = _http_request.content_length; // post正文长度
    auto &responce_body = _http_responce.responce_body;  // 响应正文

    std::string query_string_env;   // GET传参时需要的环境变量
    std::string method_env;         //  请求方式环境变量
    std::string content_length_env; // Post传参时需要的环境变量

    // 1. 站在父进程角度创建两个匿名管道
    int input[2];
    int output[2];
    if (pipe(input) < 0) // 创建失败
    {
        LOG(ERROR, "pipe input error!");
        code = SERVER_ERROR;
        return code;
    }
    if (pipe(output) < 0) // 创建失败
    {
        // 此时input是成功的，需要关闭
        close(input[0]);
        close(input[1]);

        LOG(ERROR, "pipe output error!");
        code = SERVER_ERROR;
        return code;
    }

    // 2. 创建子进程
    pid_t pid = fork();
    if (pid == 0) // 子进程
    {
        // 关闭不需要的权限
        close(input[0]);
        close(output[1]);

        // 进程替换只会替换代码+数据，不会新建PCB，会保留原先的系统资源
        // 但进程地址空间的数据，比如局部变量，全局变量，会消失
        // 所以我们约定，替换后的进程，读取管道等价于读取标准输入0，写入管道，等价于写到标准输出
        // 即替换之前，对子进程进行重定向
        // input[1]: 子进程写入， 子进程 写到1 --》 子进程写到 input[1];
        // output[0]：子进程读取，子进程 读取0 --》 子进程读取 output[0];
        dup2(input[1], 1);  // 本来fd1指向标准输出，现在指向input[1]
        dup2(output[0], 0); // 同理
        // 之后的测试输出都需要更改cout->cerr

        // 3. 确定获取参数的方法
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
            #ifdef DEBUG
            LOG(DEBUG, "Get Method: GET, add query_string_env ..."); //这里需要更改LOG：cout->cerr
            #endif
        }
        else if ("POST" == method)
        {
            content_length_env = "CONTENT_LENGTH=";
            content_length_env += std::to_string(content_length);
            putenv((char *)content_length_env.c_str());
            #ifdef DEBUG
            LOG(DEBUG, "Get Method: POST, add qcontent_length_env ..."); //这里需要更改LOG：cout->cerr
            #endif
        }
        else
        {
            ;
        }

        execl(bin.c_str(), bin.c_str(), nullptr); // 此时bin已经能够获取参数

        // 子进程不需要手动释放fd，结束进程后自动释放

        LOG(ERROR, "execl error!");
        exit(1);
    }
    else if (pid < 0) // fork error
    {
        LOG(WARNING, "fork error!");
        return SERVER_ERROR;
    }
    else // parent
    {
        // 关闭不需要的权限
        close(input[1]);
        close(output[0]);

        // 3. 传参
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
            ; // 在子进程{}设置
            // 环境变量具有全局属性，则子进程是能够继承到父进程的环境变量的
            // 且环境变量不受execp程序替换的影响
        }

        // 4. 读取cgi返回结果，构建正文
        char ch = 0;
        while (read(input[0], &ch, 1) > 0)
        {
            responce_body.push_back(ch);
        }
        int status = 0;
        pid_t ret = waitpid(pid, &status, 0); // 阻塞等
        if (ret == pid)
        {
            if (WIFEXITED(status)) // 正常退出
            {
                if (WEXITSTATUS(status) == 0) // 且退出码为0
                {
                    code = OK;
                }
                else
                {
                    code = BAD_REQUEST;
                }
            }
            else
            {
                code = SERVER_ERROR;
            }
        }

        // 释放资源
        close(input[0]);
        close(output[1]);
    }
    return code;
}
~~~

##### Non-Cgi

~~~C++
int ProcessNonCgi() // No cgi ： 打开静态资源
{
    LOG(INFO, "No Cgi...");
    _http_responce.src_fd = open(_http_request.path.c_str(), O_RDONLY); // 只读
    if (_http_responce.src_fd >= 0)
    {
        #ifdef DEBUG
        LOG(DEBGU, _http_request.path + " is open...");
        #endif
        struct stat st;
        stat(_http_request.path.c_str(), &st);
        _http_responce.src_size = st.st_size;
        return OK;
    }
    else
    {
        LOG(WARNING, "scr_file open error!");
        return NOT_FOUND;
    }
}
~~~

#### 构建响应

~~~C++
void BuildOkResponce()
{
    #ifdef DEBUG
    LOG(DEBUG, "BulidOkResponce");
    #endif
    // 构建响应报头
    std::string header_line;
    header_line = "Content-Type: "; //正文类型
    header_line += Suffix2Desc(_http_request.suffix);
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
void HandlerError(std::string page) // 错误处理
{
    #ifdef DEBUG
    LOG(DEBUG, "HandlerError");
    #endif
    // 返回给用户对应的错误页面
    // 此时静态资源发生改变， 处理方式发生改变
    // 所以需要更改很多成员变量
    _http_request.cgi = false;                            // 此时处理方式强制为no cgi
    _http_request.suffix = ".html";                       // 静态资源强制为 html
    _http_responce.src_fd = open(page.c_str(), O_RDONLY); // 打开对应错误处理文件
    if (_http_responce.src_fd >= 0)
    {
        struct stat st;
        stat(page.c_str(), &st);
        _http_responce.src_size = st.st_size; // 更新资源大小
        // 构建响应报头
        std::string header_line;
        header_line = "Content-Type: text/html"; // 正文类型
        header_line += LINE_END;
        _http_responce.responce_header.push_back(header_line);
        header_line = "Content-Length: "; // 正文长度
        header_line += std::to_string(_http_responce.src_size);
        header_line += LINE_END;
        _http_responce.responce_header.push_back(header_line);
    }
    else
    {
        LOG(WARNING, "open error!");
    }
}
void BuildHttpResponseHelper() // 构建处理
{
    auto &code = _http_responce.status_code; // code
    // 构建状态行
    auto &responce_line = _http_responce.responce_line;
    responce_line += HTTP_VERSION;
    responce_line += " ";
    responce_line += std::to_string(code);
    responce_line += " ";
    responce_line += Code2Desc(code);
    responce_line += LINE_END;

    // 构建响应正文，可能有响应报头
    switch (code)
    {
        case OK:
            BuildOkResponce();
            break;
        case BAD_REQUEST:
            HandlerError(PAGE_400);
            break;
        case NOT_FOUND:
            HandlerError(PAGE_404);
            break;
        case SERVER_ERROR:
            HandlerError(PAGE_500);
            break;
        default:
            break;
    }
}
~~~

### 发送响应

~~~C++
void SendHttpResponse()// 发送响应
{
    // 发送时，如果对方不读了，会产生sigpipe信号，使进程终止
    // 所以，在httpserver对象初始化时，需要忽略掉sigpipe信号，避免http进程被终止

    #ifdef DEBUG
    LOG(DEBUG, std::to_string(_http_responce.status_code));
    if (_http_request.cgi)
    {
        LOG(DEBUG, "body_size: " + std::to_string(_http_responce.responce_body.size()));
    }
    else
    {
        LOG(DEBUG, "src_fd: " + std::to_string(_http_responce.src_fd));
    }
    #endif

    send(_sock, _http_responce.responce_line.c_str(), _http_responce.responce_line.size(), 0); // 状态行
    for (auto iter : _http_responce.responce_header)                                           // 报头
    {
        send(_sock, iter.c_str(), iter.size(), 0);
    }
    send(_sock, _http_responce.blank.c_str(), _http_responce.blank.size(), 0); // 空行
    if (_http_request.cgi)                                                     // cgi 发送正文
    {
        auto &responce_body = _http_responce.responce_body;
        int size = 0;
        int total = 0;
        const char *start = responce_body.c_str();
        while (total < responce_body.size() && (size = send(_sock, start + total, responce_body.size() - total, 0)) > 0)
        {
            total += size;
        }
    }
    else // no cgi 发送正文
    {
        sendfile(_sock, _http_responce.src_fd, nullptr, _http_responce.src_size);
        close(_http_responce.src_fd);
    }
}
~~~

## 知识点

### TcpServer

地址复用

为什么没有粘包问题

INADDR_ANY

单例模式

### ThreadPool+生产者消费者模型+Task

互斥锁

条件变量

临界区管理

回调函数

单例模式

### HttpServer

忽略SIGPIPE信号

### HTTP协议



### Makefile+shell脚本



### Http处理+Util

**recv窥探宏**

格式控制

流提取stringstream

字符转换transform

**stat**

Web根目录wwwroot

CGI机制

环境变量+fork+进程替换+进程通信(匿名管道)

进程等待+status
