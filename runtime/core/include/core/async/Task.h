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

        void ResetTask();
        tf::AsyncTask GetTask() const { return handle; }

    protected:
        virtual bool DoWork() = 0;
        virtual void OnComplete(bool result) {}
        virtual void PrepareWork() {}

        friend class TaskExecutor;
        tf::AsyncTask handle;
        std::vector<tf::AsyncTask> dependencies;
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
