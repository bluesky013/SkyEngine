//
// Created by Zach Lee on 2022/10/19.
//

#include <vulkan/AsyncTransferQueue.h>
#include <vulkan/Queue.h>

namespace sky::drv {

    bool AsyncTransferQueue::HasTask()
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        return !taskQueue.empty();
    }

    bool AsyncTransferQueue::EmitSingleTask()
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
        lastTaskId.store(task.taskId);
        return true;
    }

    void AsyncTransferQueue::ThreadMain()
    {
        while (!exit.load()) {
            if (!EmitSingleTask()) {
                std::unique_lock<std::mutex> lock(mutex);
                while(!exit.load() && !HasTask()) {
                    cv.wait(lock);
                }
            }
        }
    }

    AsyncTransferQueue::~AsyncTransferQueue() noexcept
    {

    }

    void AsyncTransferQueue::Setup()
    {
        thread = std::thread(&AsyncTransferQueue::ThreadMain, this);
    }

    void AsyncTransferQueue::Shutdown()
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

}
