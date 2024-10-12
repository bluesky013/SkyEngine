//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>

namespace sky {

    void Task::StartAsync()
    {
        CounterPtr<Task> thisTask = this;
        handle = TaskExecutor::Get()->GetExecutor().dependent_async([thisTask]() {
            bool result = thisTask->DoWork();
            thisTask->OnComplete(result);
        }).first;
    }

    void TaskExecutor::ExecuteTask(const TaskPtr &task)
    {
        task->handle = executor.dependent_async([task]() {
            bool result = task->DoWork();
            task->OnComplete(result);
        }).first;
    }

    void TaskExecutor::WaitForAll()
    {
        executor.wait_for_all();
    }
} // namespace sky