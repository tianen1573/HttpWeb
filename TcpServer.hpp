#pragma once

#include <iostream>

#include <cstdlib>
#include <cassert>

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstring>

#define BACKLOG 5

class TcpServer
{
private:
    TcpServer(uint16_t port)
        : _port(port)
        , _listen_sock(-1)
    {
    }
    TcpServer(const TcpServer & s){}

public:
    static TcpServer* GetInstance(int port)//单例模式
    {
        static pthread_mutex_t mtx_svr = PTHREAD_MUTEX_INITIALIZER;
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
    }

public:
    void InitServer()
    {
        Socket();//创建监听套接字
        Bind();//绑定套接字
        Listen();//监听

    }
    void Socket()
    {
        _listen_sock = socket(AF_INET, SOCK_STREAM, 0);//创建套接字
        if(_listen_sock < 0)
        {
            exit(1);
        }

        //在TCP连接中，先退出的一方会维持一段时间的TIME_WAIT状态，该状态下的套接字不会立即释放它所申请的资源
        //只有过了2MSL时间，才会释放资源，比如释放端口号，fd等，则这段时间内，一些系统资源是不能使用的
        //socket地址复用，不等待TIME_WAIT状态退出，强制忽略该状态，使用相应的系统资源
        //则_listen_sock，不在需要等待TIME_WAIT状态，本质是允许一个端口号绑定多个进程
        int opt = 1;
        setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    }
    void Bind()
    {
        struct sockaddr_in local;
        memset(&local, 0, sizeof local);
        local.sin_family = AF_INET;//协议类型
        local.sin_port = htons(_port);//端口号
        local.sin_addr.s_addr = INADDR_ANY;//IP
        if(bind(_listen_sock, (struct sockaddr*)&local, sizeof local) < 0)
        {
            exit(2);
        }
    }
    void Listen()
    {
        if(listen(_listen_sock, BACKLOG) < 0)
        {
            exit(3);
        }
    }

    int GetSock()
    {
        return _listen_sock;
    }

private:
    int _port;
    int _listen_sock;
    // string _ip; // 服务端不需要显示绑定IP， 绑定INADDR_ANY，可接受所有网卡的请求，并且云服务器不能绑定公网IP

    static TcpServer* svr;
};
TcpServer* TcpServer::svr = nullptr;
