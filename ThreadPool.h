#pragma once
#include <vector>
#include <future>
#include <iostream>
#include <functional>

#include "InterruptableThread.h"
#include "BlockedQueue.h"

using namespace std;


typedef function<void()> task_type;
typedef void (*FuncType)(int, int); // тип указатель на функцию, которая является эталоном для функций задач

class InterruptableThread;

class ThreadPool // пул потоков
{
private:
    int m_index;                                     // для равномерного распределения задач
    int m_thread_count;                              // количество потоков
    vector<InterruptableThread*> m_threads;         // потоки
    vector<BlockedQueue<task_type>> m_thread_queues; // очереди задач для потоков
    condition_variable m_event_holder;               // для синхронизации работы потоков
    volatile bool m_work;                            // флаг для остановки работы потоков
    mutex coutLocker;
public:
    ThreadPool();
    ~ThreadPool();
    void start();                                // запуск
    void stop();                                 // остановка
    //void push_task(FuncType f, int id, int arg); // проброс задач
    void threadFunc(int qindex, mutex& mtx);     // функция входа для потока
    void interruptPool();                        // прерывание

    template<typename F, typename... Args>
    void push_task(F&& f, Args&&... args)
    {
        int queue_to_push = m_index++ % m_thread_count;
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)..., std::ref(coutLocker));
        m_thread_queues[queue_to_push].push(move(task));
        m_event_holder.notify_one();
    }
};