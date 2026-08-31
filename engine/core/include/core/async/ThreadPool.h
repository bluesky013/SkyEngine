//
// Created on 2026/03/29.
//
#pragma once

#include <atomic>
#include <condition_variable>
#include <core/async/Semaphore.h>
#include <core/template/LockFreeQueue.h>
#include <core/template/ReferenceObject.h>
#include <core/template/SmallVector.h>
#include <core/template/SpinLock.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace sky {

    // Base class for per-thread context. Derive to attach any state to a worker thread.
    struct ThreadContext {
        ThreadContext()          = default;
        virtual ~ThreadContext() = default;

        virtual void OnAttach(uint32_t threadIndex)
        {
        }
        virtual void OnDetach()
        {
        }
    };

    // Factory to create a ThreadContext for each worker thread.
    using ThreadContextFactory = std::function<ThreadContext *(uint32_t threadIndex)>;

    // Lightweight inline-callable task. Replaces std::function for worker tasks
    // to shrink the task-node and queue-cell footprint. Stores callables up to
    // 40 bytes inline with a single vtable indirection.
    class TaskFunc {
    public:
        TaskFunc() = default;

        ~TaskFunc()
        {
            Reset();
        }

        TaskFunc(TaskFunc &&other) noexcept
        {
            MoveFrom(std::move(other));
        }

        TaskFunc &operator=(TaskFunc &&other) noexcept
        {
            if (this != &other) {
                Reset();
                MoveFrom(std::move(other));
            }
            return *this;
        }

        TaskFunc(const TaskFunc &)            = delete;
        TaskFunc &operator=(const TaskFunc &) = delete;

        template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, TaskFunc>>>
        TaskFunc(F &&func) // NOLINT
        {
            Construct(std::forward<F>(func));
        }

        template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, TaskFunc>>>
        TaskFunc &operator=(F &&func)
        {
            Reset();
            Construct(std::forward<F>(func));
            return *this;
        }

        explicit operator bool() const
        {
            return vtable != nullptr;
        }

        void operator()(ThreadContext &ctx)
        {
            vtable->invoke(storage, ctx);
        }

    private:
        struct VTable {
            void (*invoke)(void *storage, ThreadContext &ctx);
            void (*move)(void *dst, void *src);
            void (*destroy)(void *storage);
        };

        template <typename Fn>
        static const VTable *GetVTable()
        {
            static const VTable kTable = {
                [](void *s, ThreadContext &ctx) { (*static_cast<Fn *>(s))(ctx); },
                [](void *dst, void *src) { new (dst) Fn(std::move(*static_cast<Fn *>(src))); },
                [](void *s) { static_cast<Fn *>(s)->~Fn(); },
            };
            return &kTable;
        }

        template <typename F>
        void Construct(F &&func)
        {
            using Fn = std::decay_t<F>;
            static_assert(std::is_invocable_v<Fn &, ThreadContext &>, "TaskFunc must be callable with ThreadContext&");
            static_assert(sizeof(Fn) <= kStorageSize, "TaskFunc capture is too large");
            static_assert(alignof(Fn) <= kStorageAlign, "TaskFunc capture alignment is too large");

            new (storage) Fn(std::forward<F>(func));
            vtable = GetVTable<Fn>();
        }

        void Reset()
        {
            if (vtable != nullptr) {
                vtable->destroy(storage);
                vtable = nullptr;
            }
        }

        void MoveFrom(TaskFunc &&other)
        {
            if (other.vtable != nullptr) {
                const VTable *table = other.vtable;
                table->move(storage, other.storage);
                table->destroy(other.storage);
                vtable       = table;
                other.vtable = nullptr;
            }
        }

        static constexpr size_t kStorageSize  = 40;
        static constexpr size_t kStorageAlign = alignof(std::max_align_t);

        alignas(kStorageAlign) unsigned char storage[kStorageSize]{};
        const VTable *vtable = nullptr;
    };

    // Task function that receives the per-thread context of the executing worker.
    using ThreadTask = TaskFunc;

    class ThreadPool;

    // Dependency graph node. Created via ThreadPool::CreateTask.
    // Task only becomes eligible for execution when all parents have completed.
    class TaskNode {
    public:
        ~TaskNode() = default;

        void AddRef() noexcept;
        void RemoveRef() noexcept;

        // Declare that this task depends on parent (parent must run first).
        void DependsOn(const CounterPtr<TaskNode> &parent);

        std::future<void> GetFuture();

    private:
        friend class ThreadPool;

        using ChildList = SmallVector<CounterPtr<TaskNode>, 4>;

        TaskNode(ThreadPool &pool, ThreadTask &&func);

        void TryEnqueue();
        void OnParentComplete();

        ThreadPool                         &pool;
        ThreadTask                          func;
        std::unique_ptr<std::promise<void>> promise;
        bool                                done{false};
        std::atomic_uint32_t                pendingParents{0};
        std::atomic_uint32_t                counter{0};
        SpinLock                            childLock;
        ChildList                           children;
    };

    using TaskNodePtr = CounterPtr<TaskNode>;

    class ThreadPool {
    public:
        explicit ThreadPool(uint32_t threadCount);
        ThreadPool(uint32_t threadCount, const ThreadContextFactory &factory);
        ~ThreadPool();

        // Submit a task to a worker queue (round-robin + work-stealing).
        void Schedule(ThreadTask &&task);

        // Submit a single task. Returns a future that signals completion.
        std::future<void> Dispatch(ThreadTask &&task);

        // Create a task node for dependency graph building. Call Submit() after setting up edges.
        TaskNodePtr CreateTask(ThreadTask &&task);

        // Submit a task node (and its transitively connected nodes) for execution.
        // Tasks with zero pending parents are enqueued immediately.
        void Submit(const TaskNodePtr &node);

        // Submit a parallel task split into taskCount chunks across all workers.
        std::future<void> Parallel(uint32_t taskCount, std::function<void(uint32_t taskIndex, uint32_t taskCount, ThreadContext &ctx)> &&task);

        // Block until every queued task has finished.
        void WaitIdle();

        uint32_t GetThreadCount() const
        {
            return threadCount;
        }
        ThreadContext *GetContext(uint32_t index) const;

    private:
        friend class TaskNode;

        void      WorkerLoop(uint32_t index);
        bool      TrySteal(uint32_t thiefIndex, ThreadTask &task);
        TaskNode *AllocNode(ThreadTask &&task);
        void      ReleaseNode(TaskNode *node);

        uint32_t                     threadCount;
        std::vector<std::thread>     threads;
        std::vector<ThreadContext *> contexts;

        static constexpr uint32_t GLOBAL_QUEUE_CAPACITY = 4096;
        static constexpr uint32_t LOCAL_QUEUE_CAPACITY  = 256;

        LockFreeQueue<ThreadTask>                globalQueue{GLOBAL_QUEUE_CAPACITY};
        std::vector<LockFreeQueue<ThreadTask> *> localQueues;

        Semaphore               wakeup{0};
        Semaphore               startupLatch{0};
        std::atomic_bool        stopping{false};
        std::atomic_uint32_t    pendingTasks{0};
        std::atomic_uint32_t    enqueueCursor{0};
        std::mutex              idleMutex;
        std::condition_variable idleCondition;

        std::vector<uint8_t *>  nodeSlabs;
        uint8_t                *currentNodeSlab  = nullptr;
        uint32_t                nodeOffset       = 0;
        uint32_t                nodeSlabCapacity = 0;
        std::vector<TaskNode *> nodeFreeList;
        SpinLock                nodePoolLock;
    };

} // namespace sky
