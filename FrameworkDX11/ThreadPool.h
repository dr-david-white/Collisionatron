// MIT License
// Copyright (c) 2025 David White
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// A simple, fairly standard, thread pool

#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <memory>

class ThreadPool
{
public:
    ThreadPool() = default;

    ThreadPool(const unsigned int numThreads)
    {
        for (unsigned int i = 0; i < numThreads; ++i)
        {
            m_workers.emplace_back([this] {
                while (true)
                {
                    std::function<void()> task;

                    // The lock must be established before waiting
                    std::unique_lock<std::mutex> lock(m_queueMutex);

                    // The condition variable's wait function will atomically unlock the mutex
                    // and suspend the thread. When woken, it re-acquires the lock.
                    m_condition.wait(lock, [this] {
                        return m_stop || !m_tasks.empty();
                        });

                    // If we woke up because we're stopping and the queue is empty, exit.
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }

                    // Otherwise, a task must be available.
                    task = std::move(m_tasks.front());
                    m_tasks.pop();

                    // Release the lock before executing the task
                    lock.unlock();

                    task();
                }
                });
        }
    }

    unsigned int threadCount() { return m_workers.size(); }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (std::thread& worker : m_workers)
        {
            worker.join();
        }
    }

    void enqueue(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_tasks.emplace(std::move(task));
        }
        m_condition.notify_one();
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    bool m_stop = false;
};

