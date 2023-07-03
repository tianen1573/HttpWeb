#pragma once

#include <iostream>

#include <pthread.h>

#include "TcpServer.hpp"
#include "Protocol.hpp"

#define PORT 8081

class HttpServer
{

public:
    HttpServer(int port = PORT)
        : _port(port), _tcp_server(nullptr), stop(false)
    {
    }
    ~HttpServer()
    {
    }

public:
    void InitServer()
    {
        _tcp_server = TcpServer::GetInstance(_port);
    }
    void Loop()
    {
        int listen_sock = _tcp_server->GetSock();
        while (!stop)
        {
            struct sockaddr_in peer;
            socklen_t len = sizeof peer;
            int sock = accept(listen_sock, (struct sockaddr*)&peer, &len);
            if(sock < 0)
            {
                continue;
            }

            int * psock = new int(sock);
            pthread_t tid;
            pthread_create(&tid, nullptr, Entrance::HandlerRequest, psock);//创建线程
            pthread_detach(tid);//线程分离

        }
    }

private:
    int _port;
    TcpServer *_tcp_server;

    bool stop;
};