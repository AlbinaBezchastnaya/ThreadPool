#include <queue>
#include <future>
#include <condition_variable>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

thread_local bool thread_interrupt_flag = false;
mutex coutLocker;

//////////////////////////////////////////////////////////////////////////////////////////////////
// class BlockedQueue 
//////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
class BlockedQueue
{
private:
    mutex m_locker;
    queue<T> m_task_queue;         // ������� �����
    condition_variable m_notifier; // �����������

public:
    void push(T& item)
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

//////////////////////////////////////////////////////////////////////////////////////////////////
// class ThreadPool
//////////////////////////////////////////////////////////////////////////////////////////////////

void taskFunc(int id, int delay);
typedef function<void()> task_type;  // ������� ����������� ��� ���������� ����
typedef void (*FuncType) (int, int); // ��� ��������� �� �������, ������� �������� �������� ��� ������� �����

class ThreadPool  // ��� �������
{
    class InterruptableThread
    {
    private:
        mutex m_defender;
        bool* m_pFlag;
        thread m_thread;
    public:
        InterruptableThread(ThreadPool* pool, int qindex) :
            m_pFlag(nullptr),
            m_thread(&InterruptableThread::startFunc, this, pool, qindex) {
        }

        ~InterruptableThread() {
            m_thread.join();
        }

        void interrupt()
        {
            lock_guard<mutex> l(m_defender);
            if (m_pFlag) // ����������, ���� �� �������, ���� �� �����
                *m_pFlag = true;
        }

        void startFunc(ThreadPool* pool, int qindex)
        {
            {
                lock_guard<mutex> l(m_defender);
                m_pFlag = &thread_interrupt_flag;
            }
            pool->threadFunc(qindex);
            {
                lock_guard<mutex> l(m_defender);
                m_pFlag = nullptr;
            }
        }
    };

private:
    int m_index;                        // ��� ������������ ������������� �����
    int m_thread_count;                 // ���������� �������
    vector<InterruptableThread*> m_threads;           // ������
    vector<BlockedQueue<task_type>> m_thread_queues;  // ������� ����� ��� �������
    condition_variable m_event_holder;  // ��� ������������� ������ �������
    volatile bool m_work;               // ���� ��� ��������� ������ �������
public:
    ThreadPool();
    void start();                                // ������
    void push_task(FuncType f, int id, int arg); // ������� �����
    void threadFunc(int qindex);                 // ������� ����� ��� ������
    void interrupt();                            // ���������
};

static bool checkInterrupted()
{
    lock_guard<mutex> l(coutLocker);
    cout << '-' << thread_interrupt_flag << '-';

    return thread_interrupt_flag;
}

ThreadPool::ThreadPool() :
    m_thread_count(thread::hardware_concurrency() != 0 ? thread::hardware_concurrency() : 4),
    m_thread_queues(m_thread_count) {
}
// thread::hardware_concurrency() - ���������� ���������� �������, ������� ��������� ����� ����������� ������������

void ThreadPool::interrupt()
{
    for (auto& t : m_threads) {
        t->interrupt();
    }
}

void ThreadPool::start()
{
    for (int i = 0; i < m_thread_count; i++)
        m_threads.push_back(new InterruptableThread(this, i));
}

void ThreadPool::push_task(FuncType f, int id, int arg)
{
    int queue_to_push = m_index++ % m_thread_count;
    task_type task = [=] {f(id, arg); };         // ��������� �������
    m_thread_queues[queue_to_push].push(task); // ������ � �������
}

void ThreadPool::threadFunc(int qindex)
{
    while (true) // ��������� ��������� ������
    {
        if (checkInterrupted())
        {
            lock_guard<mutex> l(coutLocker);
            cout << "thread was interrupted" << endl;
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

//////////////////////////////////////////////////////////////////////////////////////////////////
// class RequestHandler
//////////////////////////////////////////////////////////////////////////////////////////////////

class RequestHandler
{
private:
    ThreadPool m_tpool; // ��� �������
public:
    RequestHandler();
    void pushRequest(FuncType f, int id, int arg);  // �������� ������� �� ����������
    void interruptPool();
};

RequestHandler::RequestHandler() {
    m_tpool.start();
}

void RequestHandler::pushRequest(FuncType f, int id, int arg) {
    m_tpool.push_task(f, id, arg);
}

void RequestHandler::interruptPool() {
    m_tpool.interrupt();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// main
//////////////////////////////////////////////////////////////////////////////////////////////////



int main()
{
    srand(0);
    RequestHandler rh;
    ThreadPool tpool;


    for (int i = 0; i < 20; i++)
        rh.pushRequest(taskFunc, i, 1 + rand() % 4);

    this_thread::sleep_for(chrono::seconds(8));
    rh.interruptPool();

    // for(int i=0; i<20; i++)
    //    taskFunc(i, 1 + rand()%4);

    return 0;
}

void taskFunc(int id, int delay)
{
    for (int i = 0; i < delay; i++)
    {
        if (checkInterrupted())
        {
            unique_lock<mutex> l(coutLocker);
            cout << "task " << id << " was interrupted" << endl;
            return;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }

    unique_lock<mutex> l(coutLocker);
    cout << "task " << id << " made by thread_id " << this_thread::get_id() << endl;
}