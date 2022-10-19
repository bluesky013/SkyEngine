//
// Created by Zach Lee on 2022/10/19.
//

#pragma once

#include "vulkan/CommandPool.h"
#include "vulkan/vulkan.h"
#include <thread>
#include <future>
#include <deque>
#include <functional>
#include <unordered_map>

namespace sky::drv {

    class Device;

    struct TransferTask {
        uint32_t taskId;
        std::function<void()> func;
        std::function<void(uint32_t)> callback;
    };
    using TransferTaskHandle = uint32_t;

    class AsyncTransferQueue : public DevObject {
    public:
        ~AsyncTransferQueue() noexcept;

        void Setup();
        void Shutdown();

        uint32_t GetQueueFamilyIndex() const
        {
            return queueFamilyIndex;
        }

        VkQueue GetNativeHandle() const
        {
            return queue;
        }

        template <typename T>
        TransferTaskHandle CreateTask(T&& task)
        {
            return CreateTask(std::forward<T>(task), {});
        }

        template <typename T, typename C>
        TransferTaskHandle CreateTask(T && task, C && callback)
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            ++currentTaskId;
            taskQueue.emplace_back(currentTaskId++, std::move(task), std::move(callback));
            return taskQueue.back().taskId;
        }

        bool HasComplete(TransferTaskHandle handle)
        {
            return lastTaskId.load() >= handle;
        }

    private:
        friend class Device;
        AsyncTransferQueue(Device &dev, VkQueue q, uint32_t family)
            : DevObject(dev)
            , queueFamilyIndex(family)
            , queue(q)
            , exit(false)
            , currentTaskId(0)
            , lastTaskId(0)
        {
        }

        void ThreadMain();
        bool EmitSingleTask();
        bool HasTask();

        uint32_t       queueFamilyIndex;
        VkQueue        queue;
        CommandPoolPtr pool;

        std::atomic_bool         exit;
        uint32_t                 currentTaskId;
        std::atomic_uint32_t     lastTaskId;
        std::thread              thread;
        std::mutex               taskMutex;
        std::mutex               mutex;
        std::condition_variable  cv;
        std::deque<TransferTask> taskQueue;
    };
    using AsyncTransferQueuePtr = std::unique_ptr<AsyncTransferQueue>;
} // namespace sky::drv
