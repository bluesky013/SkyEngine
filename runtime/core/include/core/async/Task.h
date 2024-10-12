//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/template/ReferenceObject.h>
#include <taskflow/taskflow.hpp>

namespace sky {

    class Task : public RefObject {
    public:
        Task() = default;
        ~Task() override = default;

        void StartAsync();

    private:
        virtual bool DoWork() = 0;
        virtual void OnComplete(bool result) {}

        friend class TaskExecutor;
        tf::AsyncTask handle;
    };
    using TaskPtr = CounterPtr<Task>;

    class TaskExecutor : public Singleton<TaskExecutor> {
    public:
        TaskExecutor() = default;
        ~TaskExecutor() override = default;

        void ExecuteTask(const TaskPtr &task);
        void WaitForAll();

        tf::Executor &GetExecutor() { return executor; }
    private:
        tf::Executor executor;
    };

} // namespace sky
