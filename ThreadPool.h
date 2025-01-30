#pragma once
#include <vector>
#include <future>
#include <iostream>
#include <functional>

#include "InterruptableThread.h"
#include "BlockedQueue.h"

using namespace std;


typedef function<void()> task_type;
typedef void (*FuncType)(int, int); // ��� ��������� �� �������, ������� �������� �������� ��� ������� �����

class InterruptableThread;

class ThreadPool // ��� �������
{
private:
    int m_index;                                     // ��� ������������ ������������� �����
    int m_thread_count;                              // ���������� �������
    vector<InterruptableThread*> m_threads;         // ������
    vector<BlockedQueue<task_type>> m_thread_queues; // ������� ����� ��� �������
    condition_variable m_event_holder;               // ��� ������������� ������ �������
    volatile bool m_work;                            // ���� ��� ��������� ������ �������
    mutex coutLocker;
public:
    ThreadPool();
    ~ThreadPool();
    void start();                                // ������
    void stop();                                 // ���������
    //void push_task(FuncType f, int id, int arg); // ������� �����
    void threadFunc(int qindex, mutex& mtx);     // ������� ����� ��� ������
    void interruptPool();                        // ����������

    template<typename F, typename... Args>
    void push_task(F&& f, Args&&... args)
    {
        int queue_to_push = m_index++ % m_thread_count;
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)..., std::ref(coutLocker));
        m_thread_queues[queue_to_push].push(move(task));
        m_event_holder.notify_one();
    }
};