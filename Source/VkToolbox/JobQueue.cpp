
// ================================================================================================
// File: VkToolbox/JobQueue.cpp
// Author: Guilherme R. Lampert
// Created on: 14/05/17
// Brief: Basic asynchronous job/task queue.
// ================================================================================================

#include "JobQueue.hpp"
#include "Log.hpp"

#if defined(WIN32) || defined(WIN64)
    #define NOMINMAX 1
    #define WIN32_LEAN_AND_MEAN 1
    #include <Windows.h>
#endif // WIN32 || WIN64

namespace VkToolbox
{

// ========================================================
// class JobQueue:
// ========================================================

JobQueue::JobQueue(const char * const threadName)
    : m_terminating{ false }
    , m_threadName{ threadName }
{
}

JobQueue::~JobQueue()
{
    if (m_worker.joinable())
    {
        waitAll();
        m_mutex.lock();
        m_terminating = true;
        m_condition.notify_one();
        m_mutex.unlock();
        m_worker.join();
    }
}

void JobQueue::launch()
{
    assert(!m_worker.joinable()); // Not already launched!
    m_worker = std::thread{ &JobQueue::queueLoop, this };
}

void JobQueue::pushJob(Job job)
{
    std::lock_guard<std::mutex> lock{ m_mutex };
    m_queue.push(std::move(job));
    m_condition.notify_one();
}

void JobQueue::waitAll()
{
    std::unique_lock<std::mutex> lock{ m_mutex };
    m_condition.wait(lock, [this]() { return m_queue.empty(); });
}

void JobQueue::queueLoop()
{
    if (m_threadName != nullptr)
    {
        setThisThreadName(m_threadName);
    }

    for (;;)
    {
        Job job;

        {
            std::unique_lock<std::mutex> lock{ m_mutex };
            m_condition.wait(lock, [this] { return !m_queue.empty() || m_terminating; });
            if (m_terminating)
            {
                break;
            }
            job = m_queue.front();
        }

        job();

        {
            std::lock_guard<std::mutex> lock{ m_mutex };
            m_queue.pop();
            m_condition.notify_one();
        }
    }
}

// ========================================================
// class JobQueuePool:
// ========================================================

void JobQueuePool::initialize()
{
    static const char * const threadNames[MaxPriorities] = {
        "HighPrioJobQueue",
        "MediumPrioJobQueue",
        "LowPrioJobQueue",
    };

    for (unsigned p = 0; p < MaxPriorities; ++p)
    {
        queues[p] = std::make_unique<JobQueue>(threadNames[p]);
        queues[p]->launch();

        if (p != MediumPriority) // this is the default
        {
            setThreadPriority(queues[p]->nativeHandle(), p);
        }
    }
}

void JobQueuePool::shutdown()
{
    for (unsigned p = 0; p < MaxPriorities; ++p)
    {
        queues[p] = nullptr;
    }
}

void JobQueuePool::waitAll()
{
    for (auto & q : queues)
    {
        q->waitAll();
    }
}

// ========================================================
// Helper functions:
// ========================================================

void setThisThreadName(const char * const name)
{
    setThreadName(nullptr, name);
}

void setThreadName(void * nativeThreadHandle, const char * const name)
{
#if defined(WIN32) || defined(WIN64)
    //
    // Ridiculous way of setting the thread name on Windows, from MSDN:
    // https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
    //
    #pragma pack(push, 8)
    struct ThreadNameInfo
    {
        DWORD  dwType;     // Must be 0x1000.
        LPCSTR szName;     // Pointer to name (in user addr space).
        DWORD  dwThreadID; // Thread ID (-1=caller thread).
        DWORD  dwFlags;    // Reserved for future use, must be zero.
    };
    #pragma pack(pop)

    ThreadNameInfo info;
    info.dwType     = 0x1000;
    info.szName     = name;
    info.dwThreadID = (nativeThreadHandle != nullptr ? ::GetThreadId(nativeThreadHandle) : DWORD(-1));
    info.dwFlags    = 0;

    __try
    {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR *>(&info));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
#endif // WIN32 || WIN64
}

void setThreadPriority(void * nativeThreadHandle, const unsigned priority)
{
#if defined(WIN32) || defined(WIN64)
    int nPriority;
    switch (priority)
    {
    case JobQueuePool::HighPriority : { nPriority = THREAD_PRIORITY_HIGHEST; break; }
    case JobQueuePool::LowPriority  : { nPriority = THREAD_PRIORITY_LOWEST;  break; }
    default                         : { nPriority = THREAD_PRIORITY_NORMAL;  break; }
    } // switch
    if (!::SetThreadPriority(nativeThreadHandle, nPriority))
    {
        Log::warningF("SetThreadPriority failed with error %#x", GetLastError());
    }
#endif // WIN32 || WIN64
}

// ========================================================

} // namespace VkToolbox
