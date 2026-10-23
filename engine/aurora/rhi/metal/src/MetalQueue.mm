//
// Aurora Metal Queue.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <MetalQueue.h>

#include <MetalBuffer.h>
#include <MetalCommandPool.h>
#include <MetalDevice.h>
#include <MetalImage.h>
#include <MetalSync.h>
#include <aurora/rhi/SubmitInfo.h>
#include <core/logger/Logger.h>

#include <algorithm>

namespace sky::aurora {

    MetalQueue::MetalQueue(MetalDevice &dev, QueueType t, void *nativeQueue) : device(dev), type(t), queue(nativeQueue)
    {
    }

    MetalQueue::~MetalQueue()
    {
        if (queue != nullptr) {
            id<MTLCommandQueue> q = (__bridge_transfer id<MTLCommandQueue>)queue;
            q                     = nil;
            queue                 = nullptr;
        }
    }

    void MetalQueue::Submit(const SubmitInfo &info)
    {
        const auto result = TrySubmit(info);
        if (result.status != QueueSubmitStatus::ACCEPTED)
            LOG_E("AuroraMetal", "queue submission failed: %u (%lld)", static_cast<unsigned>(result.error.code), result.error.nativeCode);
    }

    QueueSubmitResult MetalQueue::TrySubmit(const SubmitInfo &info) noexcept
    {
        @autoreleasepool {
            auto nativeQueue = (__bridge id<MTLCommandQueue>)queue;
            if (!nativeQueue || (info.fence && info.fence->GetDevice() != &device))
                return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            for (const auto &wait : info.waitSemaphores)
                if (!wait.semaphore || wait.semaphore->GetDevice() != &device)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            for (const auto &signal : info.signalSemaphores)
                if (!signal.semaphore || signal.semaphore->GetDevice() != &device)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
            std::vector<id<MTLCommandBuffer>> commands;
            for (auto *command : info.commandBuffers) {
                if (!command || command->GetDevice() != &device)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
                const auto *metalCommand   = static_cast<MetalCommandBuffer *>(command);
                const auto  recordingError = metalCommand->GetRecordingError();
                if (recordingError.code != GpuErrorCode::NONE)
                    return {QueueSubmitStatus::REJECTED, recordingError};
                auto native = (__bridge id<MTLCommandBuffer>)metalCommand->GetNativeHandle();
                if (!native || native.commandQueue != nativeQueue || native.status != MTLCommandBufferStatusNotEnqueued)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
                if (std::find(commands.begin(), commands.end(), native) != commands.end())
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::INVALID_ARGUMENT}};
                commands.push_back(native);
            }
            id<MTLCommandBuffer> waits = nil;
            if (!info.waitSemaphores.empty()) {
                // Waits precede recorded work; appending them to an already encoded buffer would wait too late.
                waits = [nativeQueue commandBuffer];
                if (!waits)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::OUT_OF_MEMORY}};
            }
            if (commands.empty()) {
                auto empty = [nativeQueue commandBuffer];
                if (!empty)
                    return {QueueSubmitStatus::REJECTED, {GpuErrorCode::OUT_OF_MEMORY}};
                commands.push_back(empty);
            }
            auto last = commands.back();
            if (info.fence) {
                auto           *fence    = static_cast<MetalFence *>(info.fence);
                NSMutableArray *retained = [NSMutableArray arrayWithCapacity:commands.size() + (waits ? 1 : 0)];
                if (waits)
                    [retained addObject:waits];
                for (auto command : commands)
                    [retained addObject:command];
                uint64_t   value = 0;
                const auto armed = fence->ArmSubmission((__bridge void *)retained, value);
                if (!armed.Succeeded())
                    return {QueueSubmitStatus::REJECTED, armed.error};
                [last encodeSignalEvent:(__bridge id<MTLSharedEvent>)fence->GetSharedEvent() value:value];
            }
            for (const auto &wait : info.waitSemaphores) {
                auto *semaphore = static_cast<MetalSemaphore *>(wait.semaphore);
                auto  event     = (__bridge id<MTLSharedEvent>)semaphore->GetSharedEvent();
                [waits encodeWaitForEvent:event value:semaphore->GetType() == SemaphoreType::TIMELINE ? wait.value : semaphore->GetBinaryWaitValue()];
            }
            for (const auto &signal : info.signalSemaphores) {
                auto *semaphore = static_cast<MetalSemaphore *>(signal.semaphore);
                auto  event     = (__bridge id<MTLSharedEvent>)semaphore->GetSharedEvent();
                [last encodeSignalEvent:event
                                  value:semaphore->GetType() == SemaphoreType::TIMELINE ? signal.value : semaphore->AdvanceBinarySignalValue()];
            }
            if (waits) {
                [waits commit];
                if (waits.status == MTLCommandBufferStatusError) {
                    // No commands are retried after native queue work begins.
                    for (auto command : commands)
                        [command commit];
                    return {QueueSubmitStatus::INDETERMINATE, {GpuErrorCode::NATIVE_ERROR, waits.error ? waits.error.code : 0}};
                }
            }
            for (auto command : commands)
                [command commit];
            for (auto command : commands)
                if (command.status == MTLCommandBufferStatusError)
                    return {QueueSubmitStatus::INDETERMINATE, {GpuErrorCode::NATIVE_ERROR, command.error ? command.error.code : 0}};
            return {QueueSubmitStatus::ACCEPTED, {}};
        }
    }

    void MetalQueue::WaitIdle()
    {
        id<MTLCommandQueue>  mtlQueue = (__bridge id<MTLCommandQueue>)queue;
        id<MTLCommandBuffer> cb       = [mtlQueue commandBuffer];
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
        stagingDesc.size               = total;
        stagingDesc.usage              = BufferUsageFlagBit::TRANSFER_SRC;
        stagingDesc.memory             = MemoryType::CPU_TO_GPU;
        BufferPtr staging(device.CreateBuffer(stagingDesc));
        auto     *stagingMtl = static_cast<MetalBuffer *>(staging.Get());
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
            auto     blit = cb->CreateBlitEncoder();
            uint64_t src  = 0;
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
        stagingDesc.size               = total;
        stagingDesc.usage              = BufferUsageFlagBit::TRANSFER_SRC;
        stagingDesc.memory             = MemoryType::CPU_TO_GPU;
        BufferPtr staging(device.CreateBuffer(stagingDesc));
        auto     *stagingMtl = static_cast<MetalBuffer *>(staging.Get());
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
        imageBarrier.image              = dst;
        imageBarrier.subRange.baseLevel = 0;
        imageBarrier.subRange.levels    = maxLevel + 1;
        imageBarrier.subRange.baseLayer = 0;
        imageBarrier.subRange.layers    = maxLayer + 1;
        imageBarrier.srcAccess          = AccessFlagBit::NONE;
        imageBarrier.dstAccess          = AccessFlagBit::COPY_DST;
        imageBarrier.oldLayout          = ImageLayout::UNDEFINED;
        imageBarrier.newLayout          = ImageLayout::TRANSFER_DST;
        barrier.imageBarriers.push_back(imageBarrier);
        cb->PipelineBarrier(barrier);

        {
            auto                         blit = cb->CreateBlitEncoder();
            std::vector<BufferImageCopy> regions(requests.size());
            uint64_t                     bufferOffset = 0;
            for (size_t i = 0; i < requests.size(); ++i) {
                const auto &req           = requests[i];
                auto       &region        = regions[i];
                region.bufferOffset       = bufferOffset;
                region.bufferRowLength    = req.bufferRowLength;
                region.bufferImageHeight  = req.bufferImageHeight;
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
