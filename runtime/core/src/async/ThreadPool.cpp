//
// Created by blues on 2024/9/6.
//

#include <core/async/ThreadPool.h>

namespace sky {

    ThreadPool::ThreadPool(size_t numThreads)
    {
        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this]() { workerLoop(); });
        }
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopped.store(true, std::memory_order_relaxed);
        }
        condition.notify_all();
        for (auto &worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void ThreadPool::workerLoop()
    {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                condition.wait(lock, [this]() {
                    return stopped.load(std::memory_order_relaxed) || !tasks.empty();
                });

                if (stopped.load(std::memory_order_relaxed) && tasks.empty()) {
                    return;
                }

                task = std::move(tasks.front());
                tasks.pop();
            }

            task();

            if (taskCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                std::lock_guard<std::mutex> lock(waitMutex);
                waitCondition.notify_all();
            }
        }
    }

    void ThreadPool::wait_for_all()
    {
        std::unique_lock<std::mutex> lock(waitMutex);
        waitCondition.wait(lock, [this]() {
            return taskCount.load(std::memory_order_acquire) == 0;
        });
    }

} // namespace sky
