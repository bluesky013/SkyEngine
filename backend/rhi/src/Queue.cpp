//
// Created by Zach on 2023/2/2.
//

#include <rhi/Queue.h>

namespace sky::rhi {

    Queue::Queue() : exit(false), currentTaskId(0), lastTaskId(0), type(rhi::QueueType::GRAPHICS)
    {
    }

    void Queue::StartThread()
    {
        thread = std::thread(&Queue::ThreadMain, this);
    }

    void Queue::Shutdown()
    {
        exit.store(true);
        {
            std::lock_guard<std::mutex> lock(mutex);
            cv.notify_all();
        }
        if (thread.joinable()) {
            thread.join();
        }
    }

    void Queue::Wait(TransferTaskHandle handle)
    {
        std::unique_lock<std::mutex> lock(mutex);
        taskCv.wait(lock, [&]() {return HasComplete(handle); });
    }

    void Queue::ThreadMain()
    {
        while (!exit.load()) {
            if (!EmitSingleTask()) {
                std::unique_lock<std::mutex> lock(mutex);
                while (!exit.load() && !HasTask()) {
                    cv.wait(lock);
                }
            }
        }
    }

    bool Queue::HasTask()
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        return !taskQueue.empty();
    }

    bool Queue::EmitSingleTask()
    {
        TransferTask task;
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            if (taskQueue.empty()) {
                return false;
            }
            task = std::move(taskQueue.front());
            taskQueue.pop_front();
        }

        task.func();

        {
            std::unique_lock<std::mutex> lock(mutex);
            lastTaskId.store(task.taskId);
            taskCv.notify_all();
        }
        return true;
    }

    bool Queue::HasComplete(TransferTaskHandle handle) const
    {
        return lastTaskId.load() >= handle;
    }
}
