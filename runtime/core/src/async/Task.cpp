//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>

namespace sky {

    void Task::SetCallback(ITaskCallBack *callback_)
    {
        callback = callback_;
    }

    void Task::StartAsync()
    {
        PrepareWork();

        CounterPtr<Task> thisTask = this;

        AliveWeak host = callback != nullptr ? callback->host : AliveShared{};
        handle = TaskExecutor::Get()->GetExecutor().dependent_async([thisTask, host, cb = callback]() {
            bool result = thisTask->DoWork();
            thisTask->ResetTask();

            if (host.lock()) {
                cb->OnTaskComplete(result, thisTask.Get());
            }

        }, dependencies.begin(), dependencies.end()).first;
    }

    bool Task::IsWorking() const
    {
        return !handle.empty();
    }

    void Task::ResetTask()
    {
        handle.reset();
    }

    void TaskExecutor::WaitForAll()
    {
        executor.wait_for_all();
    }
} // namespace sky