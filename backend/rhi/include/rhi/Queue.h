//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <thread>
#include <deque>
#include <functional>
#include <condition_variable>
#include <rhi/CommandBuffer.h>
#include <rhi/Buffer.h>
#include <rhi/Image.h>

namespace sky::rhi {

    using TransferTaskHandle = uint32_t;
    struct TransferTask {
        TransferTaskHandle taskId;
        std::function<void()> func;
        std::function<void(uint32_t)> callback;
    };

    class Queue {
    public:
        Queue();
        virtual ~Queue() = default;

        void StartThread();
        void Shutdown();
        void Wait(TransferTaskHandle handle);
        bool HasComplete(TransferTaskHandle handle) const;

        template <typename T>
        TransferTaskHandle CreateTask(T &&task)
        {
            return CreateTask(std::forward<T>(task), nullptr);
        }

        template <typename T, typename C>
        TransferTaskHandle CreateTask(T &&task, C &&callback)
        {
            ++currentTaskId;
            {
                std::lock_guard<std::mutex> lock(taskMutex);
                taskQueue.emplace_back(TransferTask{currentTaskId, std::move(task), std::move(callback)});
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                cv.notify_all();
            }
            return currentTaskId;
        }

        rhi::QueueType GetQueueType() const { return type; }

        virtual TransferTaskHandle UploadImage(const ImagePtr &image, const std::vector<ImageUploadRequest> &requests) = 0;
        virtual TransferTaskHandle UploadBuffer(const BufferPtr &image, const std::vector<BufferUploadRequest> &requests) = 0;

        TransferTaskHandle UploadImage(const ImagePtr &image, const ImageUploadRequest &request);
        TransferTaskHandle UploadBuffer(const BufferPtr &buffer, const BufferUploadRequest &request);

    protected:
        virtual void SetupInternal() {}
        virtual void ShutdownInternal() {}

        void ThreadMain();
        bool EmitSingleTask();
        bool HasTask();

        std::atomic_bool         exit;
        std::atomic_uint32_t     currentTaskId;
        std::atomic_uint32_t     lastTaskId;
        rhi::QueueType           type;

        std::thread              thread;
        std::mutex               taskMutex;
        std::mutex               mutex;
        std::condition_variable  cv;
        std::condition_variable  taskCv;
        std::deque<TransferTask> taskQueue;
    };

}
