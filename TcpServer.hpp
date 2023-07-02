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

#define PORT 8081
#define BACKLOG 5

class TcpServer
{
private:
    TcpServer(uint16_t port = PORT)
        : _port(port)
        , _listen_sock(-1)
    {
    }
    TcpServer(const TcpServer & s){}

public:
    static TcpServer* GetInstance(int port = PORT)
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
        Socket();
        Bind();
        Listen();

    }
    void Socket()
    {
        _listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(_listen_sock < 0)
        {
            exit(1);
        }

        //socket地址复用，不等待端口
        int opt = 1;
        setsockopt(_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    }
    void Bind()
    {
        struct sockaddr_in local;
        memset(&local, 0, sizeof local);
        local.sin_family = AF_INET;
        local.sin_port = htons(_port);
        local.sin_addr.s_addr = INADDR_ANY;
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

private:
    int _port;
    int _listen_sock;
    // string _ip; // 服务端不需要显示绑定IP， 绑定INADDR_ANY，可接受所有网卡的请求，并且云服务器不能绑定公网IP

    static TcpServer* svr;
};
TcpServer* TcpServer::svr = nullptr;
