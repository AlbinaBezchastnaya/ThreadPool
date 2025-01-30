#pragma once
#include <queue>
#include <condition_variable>
#include <thread>

using namespace std;

template<class T>
class BlockedQueue
{
private:
    mutex m_locker;
    queue<T> m_task_queue;         // очередь задач
    condition_variable m_notifier; // уведомитель

public:
    void push(const T& item)
    {
        lock_guard<mutex> lock(m_locker);

        m_task_queue.push(item);  // обычный потокобезопасный push
        m_notifier.notify_one();  // делаем оповещение, чтобы поток, вызвавший pop проснулся и забрал элемент из очереди
    }

    void pop(T& item) // блокирующий метод получения элемента из очереди
    {
        unique_lock<mutex> lock(m_locker);

        if (m_task_queue.empty())
            m_notifier.wait(lock, [this] {return !m_task_queue.empty(); }); // ждем, пока вызовут push
        item = m_task_queue.front();
        m_task_queue.pop();
    }

    bool fast_pop(T& item) // неблокирующий метод получения элемента из очереди // возвращает false, если очередь пуста
    {
        lock_guard<mutex> lock(m_locker);

        if (m_task_queue.empty())
            return false;   // просто выходим

        item = m_task_queue.front(); // забираем элемент
        m_task_queue.pop();
        return true;
    }
};