//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>

namespace sky {
    void Task::StartAsync()
    {
        PrepareWork();

        CounterPtr<Task> thisTask = this;

        handle = TaskExecutor::Get()->GetExecutor().dependent_async([thisTask]() {
            bool result = thisTask->DoWork();
            thisTask->OnComplete(result);
            thisTask->ResetTask();
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

    TaskExecutor::TaskExecutor(size_t N) : executor(N)
    {
    }

    void TaskExecutor::WaitForAll()
    {
        executor.wait_for_all();
    }
} // namespace sky