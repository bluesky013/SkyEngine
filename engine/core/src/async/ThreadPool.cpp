//
// Created on 2026/03/29.
//

#include <core/async/ThreadPool.h>
#include <core/platform/Platform.h>

#include <new>

namespace sky {

    static ThreadContext DEFAULT_CONTEXT;

    namespace {
        struct DispatchState {
            TaskFunc           task;
            std::promise<void> promise;
        };

        struct ParallelState {
            std::function<void(uint32_t, uint32_t, ThreadContext &)> fn;
            uint32_t                                                 taskIndex;
            uint32_t                                                 taskCount;
            std::shared_ptr<std::atomic_uint32_t>                    remaining;
            std::shared_ptr<std::promise<void>>                      promise;
        };
    } // namespace

    // ---- TaskNode ----

    TaskNode::TaskNode(ThreadPool &pool, ThreadTask &&func) : pool(pool), func(std::move(func))
    {
    }

    void TaskNode::DependsOn(const CounterPtr<TaskNode> &parent)
    {
        pendingParents.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<SpinLock> lock(parent->childLock);
        parent->children.push_back(this);
    }

    void TaskNode::OnParentComplete()
    {
        if (pendingParents.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            TryEnqueue();
        }
    }

    void TaskNode::TryEnqueue()
    {
        // Hold an explicit reference for the queued task; the node is released
        // when the task completes. The node is arena-backed, so the raw pointer
        // stays valid while this reference is outstanding.
        AddRef();

        TaskNode *self = this;
        pool.Schedule([self](ThreadContext &ctx) {
            self->func(ctx);

            ChildList childList;
            {
                std::lock_guard<SpinLock> lock(self->childLock);
                childList = std::move(self->children);
                if (self->promise != nullptr) {
                    self->promise->set_value();
                }
                self->done = true;
            }

            for (auto &child : childList) {
                child->OnParentComplete();
            }

            self->RemoveRef();
        });
    }

    std::future<void> TaskNode::GetFuture()
    {
        std::lock_guard<SpinLock> lock(childLock);
        if (promise == nullptr) {
            promise = std::make_unique<std::promise<void>>();
            if (done) {
                promise->set_value();
            }
        }
        return promise->get_future();
    }

    void TaskNode::AddRef() noexcept
    {
        counter.fetch_add(1, std::memory_order_relaxed);
    }

    void TaskNode::RemoveRef() noexcept
    {
        if (counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            ThreadPool &owner = pool;
            this->~TaskNode();
            owner.ReleaseNode(this);
        }
    }

    // ---- ThreadPool ----

    ThreadPool::ThreadPool(uint32_t threadCount) : ThreadPool(threadCount, nullptr)
    {
    }

    ThreadPool::ThreadPool(uint32_t threadCount, const ThreadContextFactory &factory) : threadCount(threadCount)
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

        for (auto *slab : nodeSlabs) {
            ::operator delete(slab, std::align_val_t(alignof(TaskNode)));
        }
    }

    std::future<void> ThreadPool::Dispatch(ThreadTask &&task)
    {
        auto state  = std::make_shared<DispatchState>();
        state->task = std::move(task);
        auto future = state->promise.get_future();

        ThreadTask wrapped = [state](ThreadContext &ctx) {
            state->task(ctx);
            state->promise.set_value();
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
        return TaskNodePtr(AllocNode(std::move(task)));
    }

    void ThreadPool::Submit(const TaskNodePtr &node)
    {
        if (node->pendingParents.load(std::memory_order_acquire) == 0) {
            node->TryEnqueue();
        }
    }

    void ThreadPool::Schedule(ThreadTask &&task)
    {
        const uint32_t target = enqueueCursor.fetch_add(1, std::memory_order_relaxed) % threadCount;
        pendingTasks.fetch_add(1, std::memory_order_relaxed);
        while (!localQueues[target]->TryPush(std::move(task))) {
            std::this_thread::yield();
        }
        wakeup.Signal();
    }

    TaskNode *ThreadPool::AllocNode(ThreadTask &&task)
    {
        std::lock_guard<SpinLock> lock(nodePoolLock);

        TaskNode *node = nullptr;
        if (!nodeFreeList.empty()) {
            node = nodeFreeList.back();
            nodeFreeList.pop_back();
        } else {
            if (nodeOffset >= nodeSlabCapacity) {
                constexpr uint32_t kSlabNum = 64;
                auto              *slab = static_cast<uint8_t *>(::operator new(kSlabNum * sizeof(TaskNode), std::align_val_t(alignof(TaskNode))));
                nodeSlabs.push_back(slab);
                currentNodeSlab  = slab;
                nodeOffset       = 0;
                nodeSlabCapacity = kSlabNum;
                nodeFreeList.reserve(nodeFreeList.size() + kSlabNum);
            }
            node = reinterpret_cast<TaskNode *>(currentNodeSlab + nodeOffset * sizeof(TaskNode));
            ++nodeOffset;
        }

        new (node) TaskNode(*this, std::move(task));
        return node;
    }

    void ThreadPool::ReleaseNode(TaskNode *node)
    {
        std::lock_guard<SpinLock> lock(nodePoolLock);
        nodeFreeList.push_back(node);
    }

    std::future<void> ThreadPool::Parallel(uint32_t taskCount, std::function<void(uint32_t taskIndex, uint32_t taskCount, ThreadContext &ctx)> &&task)
    {
        auto remaining     = std::make_shared<std::atomic_uint32_t>(taskCount);
        auto sharedPromise = std::make_shared<std::promise<void>>();
        auto future        = sharedPromise->get_future();

        for (uint32_t i = 0; i < taskCount; ++i) {
            auto state       = std::make_shared<ParallelState>();
            state->fn        = task; // copy per chunk
            state->taskIndex = i;
            state->taskCount = taskCount;
            state->remaining = remaining;
            state->promise   = sharedPromise;

            ThreadTask wrapped = [state](ThreadContext &ctx) {
                state->fn(state->taskIndex, state->taskCount, ctx);
                if (state->remaining->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    state->promise->set_value();
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
        idleCondition.wait(lock, [this]() { return pendingTasks.load(std::memory_order_acquire) == 0; });
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
