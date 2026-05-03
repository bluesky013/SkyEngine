//
// Aurora Metal Queue.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <MetalQueue.h>
#include <MetalDevice.h>
#include <MetalCommandPool.h>
#include <MetalSync.h>
#include <aurora/rhi/SubmitInfo.h>

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

} // namespace sky::aurora
