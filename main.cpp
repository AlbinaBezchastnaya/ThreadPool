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
    queue<T> m_task_queue;         // очередь задач
    condition_variable m_notifier; // уведомитель

public:
    void push(T& item)
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

//////////////////////////////////////////////////////////////////////////////////////////////////
// class ThreadPool
//////////////////////////////////////////////////////////////////////////////////////////////////

void taskFunc(int id, int delay);
typedef function<void()> task_type;  // удобное определение для сокращения кода
typedef void (*FuncType) (int, int); // тип указатель на функцию, которая является эталоном для функций задач

class ThreadPool  // пул потоков
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
            if (m_pFlag) // защищаемся, чтоб не попасть, куда не нужно
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
    int m_index;                        // для равномерного распределения задач
    int m_thread_count;                 // количество потоков
    vector<InterruptableThread*> m_threads;           // потоки
    vector<BlockedQueue<task_type>> m_thread_queues;  // очереди задач для потоков
    condition_variable m_event_holder;  // для синхронизации работы потоков
    volatile bool m_work;               // флаг для остановки работы потоков
public:
    ThreadPool();
    void start();                                // запуск
    void push_task(FuncType f, int id, int arg); // проброс задач
    void threadFunc(int qindex);                 // функция входа для потока
    void interrupt();                            // остановка
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
// thread::hardware_concurrency() - возвращает количество потоков, которые физически могут выполняться одновременно

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
    task_type task = [=] {f(id, arg); };         // формируем функтор
    m_thread_queues[queue_to_push].push(task); // кладем в очередь
}

void ThreadPool::threadFunc(int qindex)
{
    while (true) // обработка очередной задачи
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
            // попытка быстро забрать задачу из любой очереди, начиная со своей
            if (res = m_thread_queues[(qindex + i) % m_thread_count].fast_pop(task_to_do))
                break;
        }

        if (!res)
        {
            // вызываем блокирующее получение очереди
            m_thread_queues[qindex].pop(task_to_do);
        }
        else if (!task_to_do)
        {
            // чтобы не допустить зависания потока
            // кладем обратно задачу-пустышку
            m_thread_queues[(qindex + i) % m_thread_count].push(task_to_do);
        }

        if (!task_to_do)
            return;

        // выполняем задачу
        task_to_do();
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// class RequestHandler
//////////////////////////////////////////////////////////////////////////////////////////////////

class RequestHandler
{
private:
    ThreadPool m_tpool; // пул потоков
public:
    RequestHandler();
    void pushRequest(FuncType f, int id, int arg);  // отправка запроса на выполнение
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