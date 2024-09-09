//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>

namespace sky {

    void TaskExecutor::ExecuteTask(const TaskPtr &task)
    {
        executor.dependent_async([task]() {
            bool result = task->DoWork();
            task->OnComplete(result);
        });
    }

    void TaskExecutor::WaitForAll()
    {
        executor.wait_for_all();
    }
} // namespace sky