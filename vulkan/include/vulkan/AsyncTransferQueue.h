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
    class Queue;

    struct TransferTask {
        uint32_t taskId;
        std::function<void()> func;
        std::function<void(uint32_t)> callback;
    };
    using TransferTaskHandle = uint32_t;

    class AsyncTransferQueue : public DevObject {
    public:
        ~AsyncTransferQueue() noexcept = default;

        void Setup();
        void Shutdown();

        template <typename T>
        TransferTaskHandle CreateTask(T &&task)
        {
            return CreateTask(std::forward<T>(task), nullptr);
        }

        template <typename T, typename C>
        TransferTaskHandle CreateTask(T &&task, C &&callback)
        {
            ++currentTaskId;
            std::lock_guard<std::mutex> lock(taskMutex);
            taskQueue.emplace_back(TransferTask{currentTaskId, std::move(task), std::move(callback)});
            cv.notify_all();
            return taskQueue.back().taskId;
        }

        TransferTaskHandle UploadBuffer(const BufferPtr &buffer, const BufferUploadRequest &request);

        TransferTaskHandle UploadImage(const ImagePtr &image, const ImageUploadRequest &request);

        bool HasComplete(TransferTaskHandle handle) const
        {
            return lastTaskId.load() >= handle;
        }

        Queue *GetQueue() const
        {
            return queue;
        }

    private:
        friend class Device;
        AsyncTransferQueue(Device &dev, Queue *que)
            : DevObject(dev), queue(que), exit(false), currentTaskId(0), currentFrameId(0), mapped(nullptr), lastTaskId(0)
        {
        }

        void ThreadMain();
        bool EmitSingleTask();
        bool HasTask();

        uint64_t BeginFrame();
        void EndFrame();

        static constexpr uint64_t BLOCK_SIZE   = 16 * 1024 * 1024;
        static constexpr uint32_t INFLIGHT_NUM = 3;

        Queue *queue;

        std::atomic_bool         exit;
        uint32_t                 currentTaskId;
        uint32_t                 currentFrameId;
        uint8_t                 *mapped{nullptr};
        std::atomic_uint32_t     lastTaskId;
        BufferPtr                stagingBuffer;
        CommandBufferPtr         inflightCommands[INFLIGHT_NUM];

        std::thread              thread;
        std::mutex               taskMutex;
        std::mutex               mutex;
        std::condition_variable  cv;
        std::deque<TransferTask> taskQueue;
    };
    using AsyncTransferQueuePtr = std::unique_ptr<AsyncTransferQueue>;
} // namespace sky::drv
