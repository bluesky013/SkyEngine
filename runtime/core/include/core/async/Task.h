//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/template/ReferenceObject.h>
#include <core/async/ThreadPool.h>

namespace sky {

    struct CallBackAlive {};

    class Task;
    using TaskPtr = CounterPtr<Task>;

    class Task : public RefObject {
    public:
        Task() = default;
        ~Task() override = default;

        void StartAsync();

        bool IsWorking() const;
        void ResetTask();
        AsyncTaskHandle GetTask() const { return handle; }

    protected:
        virtual bool DoWork() = 0;
        virtual void PrepareWork() {}
        virtual void OnComplete(bool result) {};

        friend class TaskExecutor;

        std::atomic_bool                isDone{false};
        AsyncTaskHandle                 handle;
        std::vector<AsyncTaskHandle>    dependencies;

    };

    class TaskExecutor : public Singleton<TaskExecutor> {
    public:
        explicit TaskExecutor(size_t N);
        TaskExecutor() = default;
        ~TaskExecutor() override = default;

        void WaitForAll();

        ThreadPool &GetExecutor() { return executor; }
    private:
        ThreadPool executor;
    };

} // namespace sky
