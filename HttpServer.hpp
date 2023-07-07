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
    }
    void Loop()
    {
        TcpServer *signal_tcp = TcpServer::GetInstance(_port);
        ThreadPool *signal_tp = ThreadPool::GetInstance();

        LOG(INFO, "Loop begin ...");
        while (_stop == false)
        {
            struct sockaddr_in peer;
            socklen_t len = sizeof peer;// 发送方数据
            int sock = accept(signal_tcp->GetSock(), (struct sockaddr*)&peer, &len);
            if(sock < 0)
            {
                continue;
            }
            LOG(INFO, "Get a new link ...,and sock: " + std::to_string(sock));
            Task task(sock);
            signal_tp->PushTask(task);
        }
    }

private:
    int _port;
    bool _stop;

    // TcpServer *_tcp_server;
    // ThreadPool * _thread_pool;
};