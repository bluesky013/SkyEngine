//
// Created on 2026/03/29.
//

#pragma once

#include <cstdint>
#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <mutex>
#include <condition_variable>
#include <core/template/LockFreeQueue.h>
#include <core/template/ReferenceObject.h>
#include <core/async/Semaphore.h>

namespace sky {

    // Base class for per-thread context. Derive to attach any state to a worker thread.
    class ThreadContext {
    public:
        ThreadContext() = default;
        virtual ~ThreadContext() = default;

        virtual void OnAttach(uint32_t threadIndex) {}
        virtual void OnDetach() {}
    };

    // Factory to create a ThreadContext for each worker thread.
    using ThreadContextFactory = std::function<ThreadContext*(uint32_t threadIndex)>;

    // Task function that receives the per-thread context of the executing worker.
    using ThreadTask = std::function<void(ThreadContext &ctx)>;

    class ThreadPool;

    // Dependency graph node. Created via ThreadPool::CreateTask.
    // Task only becomes eligible for execution when all parents have completed.
    class TaskNode : public RefObject {
    public:
        ~TaskNode() override = default;

        // Declare that this task depends on parent (parent must run first).
        void DependsOn(const CounterPtr<TaskNode> &parent);

        std::future<void> GetFuture() { return promise.get_future(); }

    private:
        friend class ThreadPool;
        TaskNode(ThreadPool &pool, ThreadTask &&func);

        void TryEnqueue();
        void OnParentComplete();

        ThreadPool               &pool;
        ThreadTask                func;
        std::promise<void>        promise;
        std::atomic_uint32_t      pendingParents{0};
        std::mutex                childMutex;
        std::vector<CounterPtr<TaskNode>> children;
    };

    using TaskNodePtr = CounterPtr<TaskNode>;

    class ThreadPool {
    public:
        explicit ThreadPool(uint32_t threadCount);
        ThreadPool(uint32_t threadCount, const ThreadContextFactory &factory);
        ~ThreadPool();

        // Submit a single task. Returns a future that signals completion.
        std::future<void> Dispatch(ThreadTask &&task);

        // Create a task node for dependency graph building. Call Submit() after setting up edges.
        TaskNodePtr CreateTask(ThreadTask &&task);

        // Submit a task node (and its transitively connected nodes) for execution.
        // Tasks with zero pending parents are enqueued immediately.
        void Submit(const TaskNodePtr &node);

        // Submit a parallel task split into taskCount chunks across all workers.
        std::future<void> Parallel(uint32_t taskCount,
                                   std::function<void(uint32_t taskIndex, uint32_t taskCount, ThreadContext &ctx)> &&task);

        // Block until every queued task has finished.
        void WaitIdle();

        uint32_t GetThreadCount() const { return threadCount; }
        ThreadContext *GetContext(uint32_t index) const;

    private:
        friend class TaskNode;

        void Enqueue(ThreadTask &&task);
        void WorkerLoop(uint32_t index);
        bool TrySteal(uint32_t thiefIndex, ThreadTask &task);

        uint32_t                        threadCount;
        std::vector<std::thread>        threads;
        std::vector<ThreadContext*>      contexts;

        static constexpr uint32_t GLOBAL_QUEUE_CAPACITY = 4096;
        static constexpr uint32_t LOCAL_QUEUE_CAPACITY  = 256;

        LockFreeQueue<ThreadTask>                globalQueue{GLOBAL_QUEUE_CAPACITY};
        std::vector<LockFreeQueue<ThreadTask>*>  localQueues;

        Semaphore                   wakeup{0};
        Semaphore                   startupLatch{0};
        std::atomic_bool            stopping{false};
        std::atomic_uint32_t        pendingTasks{0};
        std::mutex                  idleMutex;
        std::condition_variable     idleCondition;
    };

} // namespace sky
