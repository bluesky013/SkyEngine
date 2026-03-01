//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/template/ReferenceObject.h>
#include <taskflow/taskflow.hpp>

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

        tf::AsyncTask GetTask() const { return handle; }

    protected:
        virtual bool DoWork() = 0;
        virtual void PrepareWork() {}
        virtual void OnComplete(bool result) {};

        friend class TaskExecutor;

        std::atomic_bool           isDone{false};
        tf::AsyncTask              handle;
        std::vector<tf::AsyncTask> dependencies;

    };

    class TaskExecutor : public Singleton<TaskExecutor> {
    public:
        explicit TaskExecutor(size_t N);
        TaskExecutor() = default;
        ~TaskExecutor() override = default;

        void WaitForAll();

        tf::Executor &GetExecutor() { return executor; }
    private:
        tf::Executor executor;
    };

} // namespace sky
