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
#include <algorithm>
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

    VulkanQueue::~VulkanQueue() = default;

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

    TransferTaskHandle VulkanQueue::UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests)
    {
        auto *dst = static_cast<VulkanBuffer *>(buffer);
        if (dst == nullptr || requests.empty()) {
            return 0;
        }

        uint64_t total = 0;
        for (const auto &req : requests) {
            total += req.size;
        }

        Buffer::Descriptor stagingDesc = {};
        stagingDesc.size   = total;
        stagingDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
        stagingDesc.memory = MemoryType::CPU_TO_GPU;
        BufferPtr staging(device.CreateBuffer(stagingDesc));
        auto *stagingVk = static_cast<VulkanBuffer *>(staging.Get());
        if (stagingVk == nullptr) {
            LOG_E(TAG, "failed to create staging buffer for upload");
            return 0;
        }

        uint8_t *mapped = stagingVk->Map();
        if (mapped == nullptr) {
            LOG_E(TAG, "failed to map staging buffer for upload");
            return 0;
        }
        uint64_t srcOffset = 0;
        for (const auto &req : requests) {
            req.source->ReadData(req.offset, req.size, mapped + srcOffset);
            srcOffset += req.size;
        }
        stagingVk->UnMap();

        auto *pool = device.CreateCommandPool(QueueType::TRANSFER);
        if (pool == nullptr) {
            LOG_E(TAG, "failed to create transfer command pool for upload");
            return 0;
        }

        auto *cb = pool->Allocate();
        if (cb == nullptr) {
            delete pool;
            return 0;
        }
        cb->Begin();
        {
            auto blit = cb->CreateBlitEncoder();
            uint64_t src = 0;
            for (const auto &req : requests) {
                blit->CopyBuffer(stagingVk, dst, req.size, src, req.dstOffset);
                src += req.size;
            }
        }
        cb->End();

        Fence::Descriptor fenceDesc = {};
        fenceDesc.createSignaled    = false;
        FencePtr fence(device.CreateFence(fenceDesc));

        SubmitInfo submit;
        submit.commandBuffers.push_back(cb);
        submit.fence = fence.Get();
        Submit(submit); // submit to this (transfer) queue, non-blocking

        PendingUpload pending;
        pending.fence   = std::move(fence);
        pending.staging = std::move(staging);
        pending.pool.reset(pool);

        pendingUploads.push_back(std::move(pending));
        return static_cast<TransferTaskHandle>(pendingUploads.size() - 1);
    }

    TransferTaskHandle VulkanQueue::UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests)
    {
        auto *dst = static_cast<VulkanImage *>(image);
        if (dst == nullptr || requests.empty()) {
            return 0;
        }

        uint64_t total    = 0;
        uint32_t maxLevel = 0;
        uint32_t maxLayer = 0;
        for (const auto &req : requests) {
            total += req.size;
            maxLevel = std::max(maxLevel, req.mipLevel);
            maxLayer = std::max(maxLayer, req.layer);
        }

        Buffer::Descriptor stagingDesc = {};
        stagingDesc.size   = total;
        stagingDesc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
        stagingDesc.memory = MemoryType::CPU_TO_GPU;
        BufferPtr staging(device.CreateBuffer(stagingDesc));
        auto *stagingVk = static_cast<VulkanBuffer *>(staging.Get());
        if (stagingVk == nullptr) {
            LOG_E(TAG, "failed to create staging buffer for image upload");
            return 0;
        }

        uint8_t *mapped = stagingVk->Map();
        if (mapped == nullptr) {
            LOG_E(TAG, "failed to map staging buffer for image upload");
            return 0;
        }
        uint64_t dstOffset = 0;
        for (const auto &req : requests) {
            req.source->ReadData(req.offset, req.size, mapped + dstOffset);
            dstOffset += req.size;
        }
        stagingVk->UnMap();

        auto *pool = device.CreateCommandPool(QueueType::TRANSFER);
        if (pool == nullptr) {
            LOG_E(TAG, "failed to create transfer command pool for image upload");
            return 0;
        }
        auto *cb = pool->Allocate();
        if (cb == nullptr) {
            delete pool;
            return 0;
        }

        cb->Begin();

        BarrierInfo barrier;
        barrier.srcStage = PipelineStageBit::TOP;
        barrier.dstStage = PipelineStageBit::TRANSFER;
        ImageBarrierInfo imageBarrier;
        imageBarrier.image                = dst;
        imageBarrier.subRange.baseLevel   = 0;
        imageBarrier.subRange.levels      = maxLevel + 1;
        imageBarrier.subRange.baseLayer   = 0;
        imageBarrier.subRange.layers      = maxLayer + 1;
        imageBarrier.srcAccess            = AccessFlagBit::NONE;
        imageBarrier.dstAccess            = AccessFlagBit::COPY_DST;
        imageBarrier.oldLayout            = ImageLayout::UNDEFINED;
        imageBarrier.newLayout            = ImageLayout::TRANSFER_DST;
        barrier.imageBarriers.push_back(imageBarrier);
        cb->PipelineBarrier(barrier);

        {
            auto blit = cb->CreateBlitEncoder();
            std::vector<BufferImageCopy> regions(requests.size());
            uint64_t bufferOffset = 0;
            for (size_t i = 0; i < requests.size(); ++i) {
                const auto &req    = requests[i];
                auto       &region = regions[i];
                region.bufferOffset      = bufferOffset;
                region.bufferRowLength   = req.bufferRowLength;
                region.bufferImageHeight = req.bufferImageHeight;
                region.subRange.level     = req.mipLevel;
                region.subRange.baseLayer = req.layer;
                region.subRange.layers    = 1;
                region.imageOffset        = req.imageOffset;
                region.imageExtent        = req.imageExtent;
                bufferOffset += req.size;
            }
            blit->CopyBufferToImage(stagingVk, dst, regions);
        }

        BarrierInfo postBarrier;
        postBarrier.srcStage = PipelineStageBit::TRANSFER;
        postBarrier.dstStage = PipelineStageBit::BOTTOM;
        ImageBarrierInfo postImageBarrier;
        postImageBarrier.image              = dst;
        postImageBarrier.subRange.baseLevel = 0;
        postImageBarrier.subRange.levels    = maxLevel + 1;
        postImageBarrier.subRange.baseLayer = 0;
        postImageBarrier.subRange.layers    = maxLayer + 1;
        postImageBarrier.srcAccess          = AccessFlagBit::COPY_DST;
        postImageBarrier.dstAccess          = AccessFlagBit::NONE;
        postImageBarrier.oldLayout          = ImageLayout::TRANSFER_DST;
        postImageBarrier.newLayout          = ImageLayout::SHADER_READ_ONLY;
        postBarrier.imageBarriers.push_back(postImageBarrier);
        cb->PipelineBarrier(postBarrier);

        cb->End();

        Fence::Descriptor fenceDesc = {};
        fenceDesc.createSignaled    = false;
        FencePtr fence(device.CreateFence(fenceDesc));

        SubmitInfo submit;
        submit.commandBuffers.push_back(cb);
        submit.fence = fence.Get();
        Submit(submit);

        PendingUpload pending;
        pending.fence   = std::move(fence);
        pending.staging = std::move(staging);
        pending.pool.reset(pool);

        pendingUploads.push_back(std::move(pending));
        return static_cast<TransferTaskHandle>(pendingUploads.size() - 1);
    }

    void VulkanQueue::Wait(TransferTaskHandle handle)
    {
        if (handle >= pendingUploads.size()) {
            return;
        }
        auto &pending = pendingUploads[handle];
        if (pending.fence != nullptr) {
            pending.fence->Wait();
            pending.pool.reset();
            pending.staging = nullptr;
            pending.fence   = nullptr;
        }
    }

    bool VulkanQueue::HasComplete(TransferTaskHandle handle) const
    {
        if (handle >= pendingUploads.size()) {
            return true;
        }
        const auto &pending = pendingUploads[handle];
        return pending.fence == nullptr || pending.fence->IsSignaled();
    }

} // namespace sky::aurora
