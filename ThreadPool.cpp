#include "ThreadPool.h"

ThreadPool::ThreadPool() : m_thread_count(thread::hardware_concurrency() != 0 ? thread::hardware_concurrency() : 4),
m_thread_queues(m_thread_count) {
}
// thread::hardware_concurrency() - ���������� ���������� �������, ������� ��������� ����� ����������� ������������

ThreadPool::~ThreadPool()
{
    for (auto& t : m_threads)
        delete t;
}

void ThreadPool::stop()
{
    for (int i = 0; i < m_thread_count; i++)
    {
        task_type empty_task;
        m_thread_queues[i].push(empty_task);
    }

    for (auto& t : m_threads)
        t->~InterruptableThread();

    for (auto& t : m_threads)
        delete t;
}

void ThreadPool::interruptPool()
{
    for (auto& t : m_threads)
        t->interrupt();
}

void ThreadPool::start()
{
    for (int i = 0; i < m_thread_count; i++)
        m_threads.push_back(new InterruptableThread(this, i, &coutLocker));
}

// void  ThreadPool::push_task(FuncType f, int id, int arg)
// {
//     int queue_to_push = m_index++ % m_thread_count;
//     task_type task([=]{f(id, arg);});                   
//     m_thread_queues[queue_to_push].push(task); // ������ � �������
// }

void ThreadPool::threadFunc(int qindex, mutex& mtx)
{
    while (true) // ��������� ��������� ������
    {
        if (InterruptableThread::checkInterrupted())
        {
            lock_guard<mutex> l(mtx);
            std::cout << "thread was interrupted" << std::endl;
            return;
        }

        task_type task_to_do;
        bool res;
        int i = 0;

        for (; i < m_thread_count; i++)
        {
            // ������� ������ ������� ������ �� ����� �������, ������� �� �����
            if (res = m_thread_queues[(qindex + i) % m_thread_count].fast_pop(task_to_do))
                break;
        }

        if (!res)
        {
            // �������� ����������� ��������� �������
            m_thread_queues[qindex].pop(task_to_do);
        }
        else if (!task_to_do)
        {
            // ����� �� ��������� ��������� ������
            // ������ ������� ������-��������
            m_thread_queues[(qindex + i) % m_thread_count].push(task_to_do);
        }

        if (!task_to_do)
            return;

        // ��������� ������
        task_to_do();
    }
}