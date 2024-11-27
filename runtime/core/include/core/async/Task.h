//
// Created by blues on 2024/9/6.
//

#pragma once

#include <core/environment/Singleton.h>
#include <core/template/ReferenceObject.h>
#include <taskflow/taskflow.hpp>

namespace sky {

    struct CallBackAlive {};
    using AliveShared = std::shared_ptr<CallBackAlive>;
    using AliveWeak = std::weak_ptr<CallBackAlive>;

    class Task;
    using TaskPtr = CounterPtr<Task>;

    class ITaskCallBack {
    public:
        ITaskCallBack() : host(std::make_shared<CallBackAlive>())
        {
        }
        virtual ~ITaskCallBack() = default;

        virtual void OnTaskComplete(bool result, Task* task) = 0;

    private:
        friend class Task;
        AliveShared host;
    };

    class Task : public RefObject {
    public:
        Task() = default;
        ~Task() override = default;

        void SetCallback(ITaskCallBack *callback);
        void StartAsync();

        bool IsWorking() const;
        void ResetTask();
        tf::AsyncTask GetTask() const { return handle; }

    protected:
        virtual bool DoWork() = 0;
        virtual void PrepareWork() {}

        friend class TaskExecutor;
        tf::AsyncTask handle;
        std::vector<tf::AsyncTask> dependencies;

        ITaskCallBack *callback = nullptr;
    };

    class TaskExecutor : public Singleton<TaskExecutor> {
    public:
        TaskExecutor() = default;
        ~TaskExecutor() override = default;

        void WaitForAll();

        tf::Executor &GetExecutor() { return executor; }
    private:
        tf::Executor executor;
    };

} // namespace sky
