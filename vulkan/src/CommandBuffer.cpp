//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/CommandBuffer.h"
#include "vulkan/Device.h"
#include "vulkan/Fence.h"
#include "vulkan/Semaphore.h"
#include "core/logger/Logger.h"

static const char* TAG = "Driver";

namespace sky::drv {

    CommandBuffer::CommandBuffer(Device& dev, VkCommandPool cp, VkCommandBuffer cb)
        : DevObject(dev), pool(cp), cmdBuffer(cb), fence(nullptr)
    {
    }

    CommandBuffer::~CommandBuffer()
    {
        Wait();

        if (fence != nullptr) {
            delete fence;
        }

        if (pool != VK_NULL_HANDLE && cmdBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device.GetNativeHandle(), pool, 1, &cmdBuffer);
        }
    }

    bool CommandBuffer::Init(const Descriptor& des)
    {
        if (des.needFence) {
            Fence::Descriptor fenceDes = {};
            fenceDes.flag = VK_FENCE_CREATE_SIGNALED_BIT;
            fence = device.CreateDeviceObject<Fence>(fenceDes);
            if (fence == nullptr) {
                return false;
            }
        }
        
        return true;
    }

    void CommandBuffer::Wait(uint64_t timeout)
    {
        if (fence != nullptr) {
            fence->Wait(timeout);
        }
    }

    void CommandBuffer::Begin()
    {
        if (fence != nullptr) {
            fence->Reset();
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    void CommandBuffer::End()
    {
        vkEndCommandBuffer(cmdBuffer);
    }

    void CommandBuffer::Submit(Queue& queue, const SubmitInfo& submit)
    {
        uint32_t waitSize = (uint32_t)submit.waits.size();
        std::vector<VkPipelineStageFlags> waitStages(waitSize);
        std::vector<VkSemaphore> waitSemaphores(waitSize);
        for (uint32_t i = 0; i < waitSize; ++i) {
            waitStages[i] = submit.waits[i].first;
            waitSemaphores[i] = submit.waits[i].second->GetNativeHandle();
        }
        
        uint32_t signalSize = (uint32_t)submit.signals.size();
        std::vector<VkSemaphore> signalSemaphores(signalSize);
        for (uint32_t i = 0; i < signalSize; ++i) {
            signalSemaphores[i] = submit.signals[i]->GetNativeHandle();
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        if (!submit.signals.empty()) {
            submitInfo.signalSemaphoreCount = signalSize;
            submitInfo.pSignalSemaphores = signalSemaphores.data();
        }

        if (!waitStages.empty()) {
            submitInfo.waitSemaphoreCount = waitSize;
            submitInfo.pWaitDstStageMask = waitStages.data();
            submitInfo.pWaitSemaphores = waitSemaphores.data();
        }
        vkQueueSubmit(queue.GetNativeHandle(), 1, &submitInfo, fence == nullptr ? VK_NULL_HANDLE : fence->GetNativeHandle());
    }

    VkCommandBuffer CommandBuffer::GetNativeHandle() const
    {
        return cmdBuffer;
    }
}