//
// Aurora Vulkan Queue.
//

#include <VulkanQueue.h>
#include <VulkanDevice.h>
#include <VulkanCommandPool.h>
#include <VulkanFence.h>
#include <VulkanSemaphore.h>
#include <VulkanConversion.h>
#include <aurora/rhi/SubmitInfo.h>
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanQueue::VulkanQueue(VulkanDevice &dev, QueueType t, VkQueue native, uint32_t family)
        : device(dev)
        , type(t)
        , queue(native)
        , familyIndex(family)
    {
    }

    void VulkanQueue::Submit(const SubmitInfo &info)
    {
        const auto &fn = device.GetDeviceFn();

        std::vector<VkCommandBufferSubmitInfo> cmdInfos;
        cmdInfos.reserve(info.commandBuffers.size());
        for (auto *cb : info.commandBuffers) {
            VkCommandBufferSubmitInfo ci = {};
            ci.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            ci.commandBuffer = static_cast<VulkanCommandBuffer *>(cb)->GetNativeHandle();
            cmdInfos.push_back(ci);
        }

        auto buildSemaInfos = [](const std::vector<SemaphoreSubmitInfo> &src) {
            std::vector<VkSemaphoreSubmitInfo> out;
            out.reserve(src.size());
            for (const auto &s : src) {
                auto *sema = static_cast<VulkanSemaphore *>(s.semaphore);
                VkSemaphoreSubmitInfo si = {};
                si.sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                si.semaphore   = sema != nullptr ? sema->GetNativeHandle() : VK_NULL_HANDLE;
                si.value       = s.value;
                si.stageMask   = static_cast<VkPipelineStageFlags2>(FromPipelineStageFlags(s.stageMask));
                si.deviceIndex = 0;
                out.push_back(si);
            }
            return out;
        };

        auto waitInfos   = buildSemaInfos(info.waitSemaphores);
        auto signalInfos = buildSemaInfos(info.signalSemaphores);

        VkSubmitInfo2 submit = {};
        submit.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit.waitSemaphoreInfoCount   = static_cast<uint32_t>(waitInfos.size());
        submit.pWaitSemaphoreInfos      = waitInfos.data();
        submit.commandBufferInfoCount   = static_cast<uint32_t>(cmdInfos.size());
        submit.pCommandBufferInfos      = cmdInfos.data();
        submit.signalSemaphoreInfoCount = static_cast<uint32_t>(signalInfos.size());
        submit.pSignalSemaphoreInfos    = signalInfos.data();

        VkFence fence = VK_NULL_HANDLE;
        if (info.fence != nullptr) {
            fence = static_cast<VulkanFence *>(info.fence)->GetNativeHandle();
        }

        const VkResult result = fn.vkQueueSubmit2(queue, 1, &submit, fence);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "vkQueueSubmit2 failed: %d", result);
        }
    }

    void VulkanQueue::WaitIdle()
    {
        device.GetDeviceFn().vkQueueWaitIdle(queue);
    }

} // namespace sky::aurora
