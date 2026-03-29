//
// Created on 2026/03/29.
//

#include <core/async/ThreadPool.h>
#include <core/platform/Platform.h>

namespace sky {

    static ThreadContext DEFAULT_CONTEXT;

    // ---- TaskNode ----

    TaskNode::TaskNode(ThreadPool &pool, ThreadTask &&func)
        : pool(pool)
        , func(std::move(func))
    {
    }

    void TaskNode::DependsOn(const CounterPtr<TaskNode> &parent)
    {
        pendingParents.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(parent->childMutex);
        parent->children.emplace_back(this);
    }

    void TaskNode::OnParentComplete()
    {
        if (pendingParents.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            TryEnqueue();
        }
    }

    void TaskNode::TryEnqueue()
    {
        CounterPtr<TaskNode> self(this);

        pool.Enqueue([self](ThreadContext &ctx) mutable {
            self->func(ctx);
            self->promise.set_value();

            std::vector<CounterPtr<TaskNode>> childList;
            {
                std::lock_guard<std::mutex> lock(self->childMutex);
                childList = std::move(self->children);
            }
            for (auto &child : childList) {
                child->OnParentComplete();
            }
        });
    }

    // ---- ThreadPool ----

    ThreadPool::ThreadPool(uint32_t threadCount)
        : ThreadPool(threadCount, nullptr)
    {
    }

    ThreadPool::ThreadPool(uint32_t threadCount, const ThreadContextFactory &factory)
        : threadCount(threadCount)
    {
        contexts.resize(threadCount, nullptr);
        localQueues.resize(threadCount, nullptr);

        for (uint32_t i = 0; i < threadCount; ++i) {
            localQueues[i] = new LockFreeQueue<ThreadTask>(LOCAL_QUEUE_CAPACITY);
            if (factory) {
                contexts[i] = factory(i);
            }
        }

        threads.reserve(threadCount);
        for (uint32_t i = 0; i < threadCount; ++i) {
            threads.emplace_back(&ThreadPool::WorkerLoop, this, i);
        }

        // wait until all workers have entered WorkerLoop and called OnAttach
        for (uint32_t i = 0; i < threadCount; ++i) {
            startupLatch.Wait();
        }
    }

    ThreadPool::~ThreadPool()
    {
        stopping.store(true, std::memory_order_release);
        wakeup.Signal(static_cast<int32_t>(threadCount));

        for (auto &t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        for (auto *ctx : contexts) {
            if (ctx != nullptr) {
                ctx->OnDetach();
                delete ctx;
            }
        }

        for (auto *q : localQueues) {
            delete q;
        }
    }

    std::future<void> ThreadPool::Dispatch(ThreadTask &&task)
    {
        auto promise = std::make_shared<std::promise<void>>();
        auto future  = promise->get_future();

        ThreadTask wrapped = [p = std::move(promise), fn = std::move(task)](ThreadContext &ctx) mutable {
            fn(ctx);
            p->set_value();
        };

        pendingTasks.fetch_add(1, std::memory_order_relaxed);
        while (!globalQueue.TryPush(std::move(wrapped))) {
            std::this_thread::yield();
        }
        wakeup.Signal();
        return future;
    }

    TaskNodePtr ThreadPool::CreateTask(ThreadTask &&task)
    {
        return TaskNodePtr(new TaskNode(*this, std::move(task)));
    }

    void ThreadPool::Submit(const TaskNodePtr &node)
    {
        if (node->pendingParents.load(std::memory_order_acquire) == 0) {
            node->TryEnqueue();
        }
    }

    void ThreadPool::Enqueue(ThreadTask &&task)
    {
        pendingTasks.fetch_add(1, std::memory_order_relaxed);
        while (!globalQueue.TryPush(std::move(task))) {
            std::this_thread::yield();
        }
        wakeup.Signal();
    }

    std::future<void> ThreadPool::Parallel(
        uint32_t taskCount,
        std::function<void(uint32_t taskIndex, uint32_t taskCount, ThreadContext &ctx)> &&task)
    {
        auto remaining     = std::make_shared<std::atomic_uint32_t>(taskCount);
        auto sharedPromise = std::make_shared<std::promise<void>>();
        auto future        = sharedPromise->get_future();

        for (uint32_t i = 0; i < taskCount; ++i) {
            auto fn = task; // copy per chunk
            ThreadTask wrapped = [i, taskCount, fn = std::move(fn),
                                  rem = remaining, sp = sharedPromise](ThreadContext &ctx) {
                fn(i, taskCount, ctx);
                if (rem->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    sp->set_value();
                }
            };

            // round-robin distribute to local queues
            const uint32_t target = i % threadCount;
            pendingTasks.fetch_add(1, std::memory_order_relaxed);
            while (!localQueues[target]->TryPush(std::move(wrapped))) {
                std::this_thread::yield();
            }
        }
        wakeup.Signal(static_cast<int32_t>(threadCount));
        return future;
    }

    bool ThreadPool::TrySteal(uint32_t thiefIndex, ThreadTask &task)
    {
        for (uint32_t i = 1; i < threadCount; ++i) {
            const uint32_t victim = (thiefIndex + i) % threadCount;
            if (localQueues[victim]->TryPop(task)) {
                return true;
            }
        }
        return false;
    }

    void ThreadPool::WaitIdle()
    {
        std::unique_lock<std::mutex> lock(idleMutex);
        idleCondition.wait(lock, [this]() {
            return pendingTasks.load(std::memory_order_acquire) == 0;
        });
    }

    ThreadContext *ThreadPool::GetContext(uint32_t index) const
    {
        if (index < threadCount) {
            return contexts[index];
        }
        return nullptr;
    }

    void ThreadPool::WorkerLoop(uint32_t index)
    {
        if (contexts[index] != nullptr) {
            contexts[index]->OnAttach(index);
        }
        startupLatch.Signal();

        for (;;) {
            ThreadTask task;

            // local -> steal from siblings -> global
            if (localQueues[index]->TryPop(task) || TrySteal(index, task) || globalQueue.TryPop(task)) {
                ThreadContext &ctx = contexts[index] != nullptr ? *contexts[index] : DEFAULT_CONTEXT;
                task(ctx);

                if (pendingTasks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard<std::mutex> lock(idleMutex);
                    idleCondition.notify_all();
                }
                continue;
            }

            if (stopping.load(std::memory_order_acquire)) {
                break;
            }

            wakeup.Wait();
        }
    }

} // namespace sky
