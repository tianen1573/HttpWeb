#pragma once

#include <iostream>
#include <queue>
#include <pthread.h>

#include "Task.hpp"
#include "Log.hpp"

#define NUM 6


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
