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
        // 无论是传址还是传参，都会在一次循环结束后析构，会关闭sock，则后续读取请求时，读取会失败
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