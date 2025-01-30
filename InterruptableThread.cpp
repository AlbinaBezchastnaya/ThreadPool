#include "InterruptableThread.h"
#include "ThreadPool.h"

thread_local bool thread_interrupt_flag = false;

InterruptableThread::InterruptableThread(ThreadPool* pool, int qindex, mutex* mtx)
    : m_pFlag(nullptr),
    m_thread(&InterruptableThread::startFunc, this, pool, qindex),
    mtx_pointer(mtx) {
}

InterruptableThread::~InterruptableThread()
{
    if (m_thread.joinable())
        m_thread.join();
}

void InterruptableThread::interrupt()
{
    lock_guard<mutex> l(m_defender);
    if (m_pFlag) // ����������, ���� �� �������, ���� �� �����
        *m_pFlag = true;

    this_thread::sleep_for(chrono::milliseconds(100));
    // ��� �������� ����� ����� void taskFunc ����� ��������� InterruptableThread::checkInterrupted() �� ���������� ������
    // ���� �������� ������, �� ����� ���������� � ������ � ������� � ���������� ������ �� �����;

}

void InterruptableThread::startFunc(ThreadPool* pool, int qindex)
{
    {
        lock_guard<mutex> l(m_defender);
        m_pFlag = &thread_interrupt_flag;
    }
    pool->threadFunc(qindex, *mtx_pointer);
    {
        lock_guard<mutex> l(m_defender);
        m_pFlag = nullptr;
    }
}

bool InterruptableThread::checkInterrupted()
{
    return thread_interrupt_flag;
}