//
// Created by blues on 2024/9/6.
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sky {

    class AsyncTaskHandle {
    public:
        AsyncTaskHandle() = default;

        explicit AsyncTaskHandle(std::shared_future<void> f)
            : future(std::move(f))
        {
        }

        bool empty() const { return !future.valid(); }

        void reset() { future = {}; }

        void Wait() const
        {
            if (future.valid()) {
                future.wait();
            }
        }

        bool IsReady() const
        {
            return !future.valid() ||
                   future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

    private:
        friend class ThreadPool;
        std::shared_future<void> future;
    };

    class ThreadPool {
    public:
        explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
        ~ThreadPool();

        // Submit a task and get a handle back
        template <typename Func>
        AsyncTaskHandle async(Func &&func)
        {
            auto task = std::make_shared<std::packaged_task<void()>>(std::forward<Func>(func));
            std::shared_future<void> future = task->get_future().share();

            taskCount.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(mutex);
                tasks.emplace([task]() { (*task)(); });
            }
            condition.notify_one();
            return AsyncTaskHandle(future);
        }

        // Submit a task without returning a handle (fire and forget)
        template <typename Func>
        void silent_async(Func &&func)
        {
            taskCount.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(mutex);
                tasks.emplace([fn = std::forward<Func>(func)]() mutable {
                    fn();
                });
            }
            condition.notify_one();
        }

        // Submit a task that depends on other async tasks (variadic)
        template <typename Func, typename ...Tasks>
        std::pair<AsyncTaskHandle, std::future<void>> dependent_async(Func &&func, Tasks &&...deps)
        {
            std::vector<AsyncTaskHandle> depVec;
            (depVec.emplace_back(std::forward<Tasks>(deps)), ...);
            return dependent_async_impl(std::forward<Func>(func), depVec);
        }

        // Submit a task that depends on other async tasks (range)
        template <typename Func, typename Iter>
        std::pair<AsyncTaskHandle, std::future<void>> dependent_async(Func &&func, Iter begin, Iter end)
        {
            std::vector<AsyncTaskHandle> deps(begin, end);
            return dependent_async_impl(std::forward<Func>(func), deps);
        }

        // Submit a dependent task without returning a future (fire and forget)
        template <typename Func, typename ...Tasks>
        AsyncTaskHandle silent_dependent_async(Func &&func, Tasks &&...deps)
        {
            std::vector<AsyncTaskHandle> depVec;
            (depVec.emplace_back(std::forward<Tasks>(deps)), ...);
            return silent_dependent_async_impl(std::forward<Func>(func), depVec);
        }

        // Parallel for loop: distributes [begin, end) across worker threads, calling func(index) for each index.
        // Blocks until all iterations complete.
        template <typename Index, typename Func>
        void parallel_for(Index begin, Index end, Func &&func)
        {
            if (begin >= end) {
                return;
            }

            const auto count = static_cast<size_t>(end - begin);
            const auto numWorkers = workers.size();

            if (numWorkers == 0 || count == 1) {
                for (Index i = begin; i < end; ++i) {
                    func(i);
                }
                return;
            }

            const auto chunks = std::min(count, numWorkers);
            const auto chunkSize = count / chunks;
            const auto remainder = count % chunks;

            std::vector<AsyncTaskHandle> handles;
            handles.reserve(chunks);

            Index offset = begin;
            for (size_t c = 0; c < chunks; ++c) {
                Index chunkBegin = offset;
                Index chunkEnd = chunkBegin + static_cast<Index>(chunkSize + (c < remainder ? 1 : 0));
                offset = chunkEnd;

                handles.emplace_back(async([&func, chunkBegin, chunkEnd]() {
                    for (Index i = chunkBegin; i < chunkEnd; ++i) {
                        func(i);
                    }
                }));
            }

            for (auto &h : handles) {
                h.Wait();
            }
        }

        // Wait for all submitted tasks to complete
        void wait_for_all();

        size_t NumWorkers() const { return workers.size(); }

    private:
        template <typename Func>
        std::pair<AsyncTaskHandle, std::future<void>> dependent_async_impl(Func &&func, std::vector<AsyncTaskHandle> &deps)
        {
            auto task = std::make_shared<std::packaged_task<void()>>(
                [fn = std::forward<Func>(func), dependencies = std::move(deps)]() mutable {
                    for (auto &dep : dependencies) {
                        dep.Wait();
                    }
                    fn();
                });

            std::future<void> rawFuture = task->get_future();
            std::shared_future<void> sharedFuture = rawFuture.share();
            AsyncTaskHandle handle(sharedFuture);

            taskCount.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(mutex);
                tasks.emplace([task]() { (*task)(); });
            }
            condition.notify_one();

            auto resultFuture = std::async(std::launch::deferred, [sharedFuture]() {
                sharedFuture.wait();
            });

            return {handle, std::move(resultFuture)};
        }

        template <typename Func>
        AsyncTaskHandle silent_dependent_async_impl(Func &&func, std::vector<AsyncTaskHandle> &deps)
        {
            auto task = std::make_shared<std::packaged_task<void()>>(
                [fn = std::forward<Func>(func), dependencies = std::move(deps)]() mutable {
                    for (auto &dep : dependencies) {
                        dep.Wait();
                    }
                    fn();
                });

            std::shared_future<void> future = task->get_future().share();

            taskCount.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(mutex);
                tasks.emplace([task]() { (*task)(); });
            }
            condition.notify_one();
            return AsyncTaskHandle(future);
        }

        void workerLoop();

        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> stopped{false};
        std::atomic<int> taskCount{0};
        std::mutex waitMutex;
        std::condition_variable waitCondition;
    };

} // namespace sky
