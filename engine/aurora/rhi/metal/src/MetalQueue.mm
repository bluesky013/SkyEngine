//
// Aurora Metal Queue.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <MetalQueue.h>
#include <MetalBuffer.h>
#include <MetalDevice.h>
#include <MetalCommandPool.h>
#include <MetalSync.h>
#include <aurora/rhi/SubmitInfo.h>

#include <algorithm>

namespace sky::aurora {

    MetalQueue::MetalQueue(MetalDevice &dev, QueueType t, void *nativeQueue)
        : device(dev)
        , type(t)
        , queue(nativeQueue)
    {
    }

    MetalQueue::~MetalQueue()
    {
        if (queue != nullptr) {
            id<MTLCommandQueue> q = (__bridge_transfer id<MTLCommandQueue>)queue;
            q = nil;
            queue = nullptr;
        }
    }

    void MetalQueue::Submit(const SubmitInfo &info)
    {
        if (info.commandBuffers.empty() && info.signalSemaphores.empty() && info.fence == nullptr) {
            return;
        }

        id<MTLCommandQueue> mtlQueue = (__bridge id<MTLCommandQueue>)queue;

        // Acquire underlying MTLCommandBuffer for each user CommandBuffer; if the
        // batch is empty but signals/fence requested, create one no-op cmdbuffer.
        std::vector<id<MTLCommandBuffer>> mtlCmdBufs;
        mtlCmdBufs.reserve(info.commandBuffers.empty() ? 1 : info.commandBuffers.size());

        for (auto *cb : info.commandBuffers) {
            auto *metalCB = static_cast<MetalCommandBuffer *>(cb);
            id<MTLCommandBuffer> raw = (__bridge id<MTLCommandBuffer>)metalCB->GetNativeHandle();
            if (raw != nil) {
                mtlCmdBufs.push_back(raw);
            }
        }

        if (mtlCmdBufs.empty()) {
            id<MTLCommandBuffer> empty = [mtlQueue commandBuffer];
            mtlCmdBufs.push_back(empty);
        }

        // Encode wait events on the FIRST cmdbuffer.
        if (!info.waitSemaphores.empty()) {
            id<MTLCommandBuffer> first = mtlCmdBufs.front();
            for (const auto &w : info.waitSemaphores) {
                auto *sema = static_cast<MetalSemaphore *>(w.semaphore);
                if (sema == nullptr) continue;
                id<MTLSharedEvent> ev = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
                const uint64_t value = (sema->GetType() == SemaphoreType::TIMELINE)
                                           ? w.value
                                           : sema->GetBinaryWaitValue();
                [first encodeWaitForEvent:ev value:value];
            }
        }

        // Encode signal events on the LAST cmdbuffer.
        if (!info.signalSemaphores.empty()) {
            id<MTLCommandBuffer> last = mtlCmdBufs.back();
            for (const auto &s : info.signalSemaphores) {
                auto *sema = static_cast<MetalSemaphore *>(s.semaphore);
                if (sema == nullptr) continue;
                id<MTLSharedEvent> ev = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
                const uint64_t value = (sema->GetType() == SemaphoreType::TIMELINE)
                                           ? s.value
                                           : sema->AdvanceBinarySignalValue();
                [last encodeSignalEvent:ev value:value];
            }
        }

        // Fence completion: encode a signal on a private MTLSharedEvent that the
        // fence's listener flips to host-visible "signaled".
        if (info.fence != nullptr) {
            auto *fence = static_cast<MetalFence *>(info.fence);
            const uint64_t v = fence->TakeNextValue();
            id<MTLSharedEvent> ev = (__bridge id<MTLSharedEvent>)fence->GetSharedEvent();
            [mtlCmdBufs.back() encodeSignalEvent:ev value:v];
        }

        for (id<MTLCommandBuffer> cb : mtlCmdBufs) {
            [cb commit];
        }
    }

    void MetalQueue::WaitIdle()
    {
        id<MTLCommandQueue> mtlQueue = (__bridge id<MTLCommandQueue>)queue;
        id<MTLCommandBuffer> cb = [mtlQueue commandBuffer];
        if (cb == nil) {
            return;
        }
        [cb commit];
        [cb waitUntilCompleted];
    }

    TransferTaskHandle MetalQueue::UploadBuffer(Buffer *buffer, const std::vector<BufferUploadRequest> &requests)
    {
        auto *dst = static_cast<MetalBuffer *>(buffer);
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
        auto *stagingMtl = static_cast<MetalBuffer *>(staging.Get());
        if (stagingMtl == nullptr) {
            return 0;
        }

        uint8_t *mapped = stagingMtl->Map();
        if (mapped == nullptr) {
            return 0;
        }
        uint64_t srcOffset = 0;
        for (const auto &req : requests) {
            req.source->ReadData(req.offset, req.size, mapped + srcOffset);
            srcOffset += req.size;
        }
        // Metal Shared-storage buffers need no unmap (MetalBuffer::UnMap is a no-op).

        auto *pool = device.CreateCommandPool(QueueType::TRANSFER);
        if (pool == nullptr) {
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
                blit->CopyBuffer(stagingMtl, dst, req.size, src, req.dstOffset);
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
        Submit(submit); // submit to this queue, non-blocking

        PendingUpload pending;
        pending.fence   = std::move(fence);
        pending.staging = std::move(staging);
        pending.pool.reset(pool);

        pendingUploads.push_back(std::move(pending));
        return static_cast<TransferTaskHandle>(pendingUploads.size() - 1);
    }

    TransferTaskHandle MetalQueue::UploadImage(Image *image, const std::vector<ImageUploadRequest> &requests)
    {
        auto *dst = static_cast<MetalImage *>(image);
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
        auto *stagingMtl = static_cast<MetalBuffer *>(staging.Get());
        if (stagingMtl == nullptr) {
            return 0;
        }

        uint8_t *mapped = stagingMtl->Map();
        if (mapped == nullptr) {
            return 0;
        }
        uint64_t dstOffset = 0;
        for (const auto &req : requests) {
            req.source->ReadData(req.offset, req.size, mapped + dstOffset);
            dstOffset += req.size;
        }
        // Metal Shared-storage buffers need no unmap.

        auto *pool = device.CreateCommandPool(QueueType::TRANSFER);
        if (pool == nullptr) {
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
            blit->CopyBufferToImage(stagingMtl, dst, regions);
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

    void MetalQueue::Wait(TransferTaskHandle handle)
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

    bool MetalQueue::HasComplete(TransferTaskHandle handle) const
    {
        if (handle >= pendingUploads.size()) {
            return true;
        }
        const auto &pending = pendingUploads[handle];
        return pending.fence == nullptr || pending.fence->IsSignaled();
    }

} // namespace sky::aurora
