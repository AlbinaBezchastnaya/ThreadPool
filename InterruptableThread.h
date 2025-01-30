#pragma once

#include <thread>
#include <mutex>

#include "ThreadPool.h"

using namespace std;

class ThreadPool;

class InterruptableThread
{
private:
    mutex* mtx_pointer;
    mutex m_defender;
    bool* m_pFlag;
    thread m_thread;

public:
    InterruptableThread(ThreadPool* pool, int qindex, mutex* mtx);
    ~InterruptableThread();
    void interrupt();
    void startFunc(ThreadPool* pool, int qindex);
    static bool checkInterrupted();
};