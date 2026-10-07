//
// Created by blues on 2024/9/6.
//

#include <core/async/Task.h>

#include <algorithm>
#include <thread>

namespace sky {
    void Task::StartAsync()
    {
        if (IsWorking()) {
            return;
        }

        PrepareWork();

        CounterPtr<Task> thisTask = this;
        isDone.store(false);

        handle = TaskExecutor::Get()->GetPool().CreateTask([thisTask](ThreadContext &) {
            const bool result = thisTask->DoWork();
            thisTask->OnComplete(result);
            thisTask->isDone.store(true);
        });

        for (const auto &dep : dependencies) {
            handle->DependsOn(dep);
        }
        dependencies.clear();

        TaskExecutor::Get()->GetPool().Submit(handle);
    }

    bool Task::IsWorking() const
    {
        return handle != nullptr && !isDone.load();
    }

    void Task::ResetTask()
    {
        handle = nullptr;
        isDone.store(false);
        dependencies.clear();
    }

    TaskExecutor::TaskExecutor(size_t N)
    {
        const uint32_t count = N != 0 ? static_cast<uint32_t>(N)
                                      : std::max<uint32_t>(1, std::thread::hardware_concurrency());
        pool = std::make_unique<ThreadPool>(count);
    }

    void TaskExecutor::WaitForAll()
    {
        pool->WaitIdle();
    }
} // namespace sky
