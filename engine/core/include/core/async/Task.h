//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/async/ThreadPool.h>
#include <core/environment/Singleton.h>
#include <core/template/ReferenceObject.h>
#include <atomic>
#include <memory>
#include <vector>

namespace sky {

    // Dependency handle: the node returned by ThreadPool::CreateTask.
    using TaskHandle = TaskNodePtr;

    class Task : public RefObject {
    public:
        Task() = default;
        ~Task() override = default;

        void StartAsync();

        bool IsWorking() const;
        void ResetTask();

        TaskHandle GetTask() const { return handle; }

    protected:
        virtual bool DoWork() = 0;
        virtual void PrepareWork() {}
        virtual void OnComplete(bool result) {}

        std::atomic_bool        isDone{false};
        TaskNodePtr             handle;
        std::vector<TaskHandle> dependencies;
    };

    class TaskExecutor : public Singleton<TaskExecutor> {
    public:
        // N == 0 uses hardware_concurrency() (min 1).
        explicit TaskExecutor(size_t N = 0);
        ~TaskExecutor() override = default;

        void WaitForAll();

        ThreadPool &GetPool() { return *pool; }

    private:
        std::unique_ptr<ThreadPool> pool;
    };

} // namespace sky
