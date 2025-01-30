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
    queue<T> m_task_queue;         // ������� �����
    condition_variable m_notifier; // �����������

public:
    void push(const T& item)
    {
        lock_guard<mutex> lock(m_locker);

        m_task_queue.push(item);  // ������� ���������������� push
        m_notifier.notify_one();  // ������ ����������, ����� �����, ��������� pop ��������� � ������ ������� �� �������
    }

    void pop(T& item) // ����������� ����� ��������� �������� �� �������
    {
        unique_lock<mutex> lock(m_locker);

        if (m_task_queue.empty())
            m_notifier.wait(lock, [this] {return !m_task_queue.empty(); }); // ����, ���� ������� push
        item = m_task_queue.front();
        m_task_queue.pop();
    }

    bool fast_pop(T& item) // ������������� ����� ��������� �������� �� ������� // ���������� false, ���� ������� �����
    {
        lock_guard<mutex> lock(m_locker);

        if (m_task_queue.empty())
            return false;   // ������ �������

        item = m_task_queue.front(); // �������� �������
        m_task_queue.pop();
        return true;
    }
};