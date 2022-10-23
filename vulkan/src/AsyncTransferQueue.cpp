//
// Created by Zach Lee on 2022/10/19.
//

#include <vulkan/AsyncTransferQueue.h>
#include <vulkan/Queue.h>
#include <vulkan/Device.h>
#include <algorithm>

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

    void AsyncTransferQueue::Setup()
    {
        thread = std::thread(&AsyncTransferQueue::ThreadMain, this);

        CreateTask([this]() {
            Buffer::Descriptor bufferInfo = {};
            bufferInfo.size = 16;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory = VMA_MEMORY_USAGE_CPU_ONLY;

            stagingBuffer = device.CreateDeviceObject<drv::Buffer>(bufferInfo);
            mapped = stagingBuffer->Map();

            for (uint32_t i = 0; i < INFLIGHT_NUM; ++i) {
                inflightCommands[i] = queue->AllocateCommandBuffer({});
            }
        });
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
        stagingBuffer->UnMap();
        stagingBuffer = nullptr;
    }

    uint64_t AsyncTransferQueue::BeginFrame()
    {
        inflightCommands[currentFrameId]->Wait();
        inflightCommands[currentFrameId]->Begin();
        return currentFrameId * BLOCK_SIZE;
    }

    void AsyncTransferQueue::EndFrame()
    {
        inflightCommands[currentFrameId]->End();
        inflightCommands[currentFrameId]->Submit(*queue, {});
        currentFrameId = (currentFrameId + 1) % INFLIGHT_NUM;
    }

    TransferTaskHandle AsyncTransferQueue::UploadBuffer(const BufferPtr &buffer, const BufferRequest &request)
    {
        return CreateTask([this, buffer, request]() {
            uint32_t copyNum   = static_cast<uint32_t>(std::ceil(request.size / static_cast<double>(BLOCK_SIZE)));
            uint64_t size      = request.size;
            uint64_t dstOffset = 0;

            const uint8_t *src = request.data + request.offset;
            for (uint32_t i = 0; i < copyNum; ++i) {
                VkBufferCopy copy = {};
                copy.size         = std::min(BLOCK_SIZE, size);
                copy.srcOffset    = BeginFrame();
                copy.dstOffset    = dstOffset;

                uint64_t offset = BeginFrame();
                memcpy(mapped + offset, src + copy.size, copy.size);

                inflightCommands[currentFrameId]->Copy(stagingBuffer, buffer, copy);

                EndFrame();

                dstOffset += BLOCK_SIZE;
                size -= BLOCK_SIZE;
            }
        });
    }

    TransferTaskHandle AsyncTransferQueue::UploadImage(const ImagePtr &image, const ImageRequest &request)
    {
        return CreateTask([]() {

            VkBufferImageCopy copy;

        });
    }

}
