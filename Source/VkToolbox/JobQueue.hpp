#pragma once

// ================================================================================================
// File: VkToolbox/JobQueue.hpp
// Author: Guilherme R. Lampert
// Created on: 14/05/17
// Brief: Basic asynchronous job/task queue.
// ================================================================================================

#include "InPlaceFunction.hpp"

#include <array>
#include <queue>
#include <memory>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace VkToolbox
{

// ========================================================
// class JobQueue:
// ========================================================

class JobQueue final
{
public:

    using Job = InPlaceFunction32<void()>;

    explicit JobQueue(const char * threadName);
    ~JobQueue();

    // Launch the worker thread.
    void launch();

    // Add a new job to the thread's queue.
    void pushJob(Job job);

    // Wait until all work items have been completed.
    void waitAll();

    // Platform-specific handle of the worker thread. On Windows, has as a HANDLE.
    void * nativeHandle() { return m_worker.native_handle(); }

private:

    // Loop through all remaining jobs. This is the worker thread entry point.
    void queueLoop();

    bool                    m_terminating;
    std::thread             m_worker;
    std::mutex              m_mutex;
    std::condition_variable m_condition;
    std::queue<Job>         m_queue;
    const char * const      m_threadName;
};

// ========================================================
// class JobQueuePool:
// ========================================================

class JobQueuePool final
{
public:

    enum ThreadPriority
    {
        HighPriority,
        MediumPriority,
        LowPriority,

        // Number of entries in the enum - internal use.
        MaxPriorities
    };

    // One for each priority.
    std::array<std::unique_ptr<JobQueue>, MaxPriorities> queues;

    void initialize();
    void shutdown();
    void waitAll();
};

// ========================================================

void setThisThreadName(const char * name);
void setThreadName(void * nativeThreadHandle, const char * name);
void setThreadPriority(void * nativeThreadHandle, unsigned priority);

// ========================================================

} // namespace VkToolbox
