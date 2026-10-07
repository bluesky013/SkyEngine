//
// Created on 2026/04/02.
//

#include "MetalDevice.h"
#include "MetalImage.h"
#include "MetalSwapChain.h"
#include "MetalSync.h"
#include "MetalUtils.h"
#include <core/logger/Logger.h>

#import <QuartzCore/CAMetalLayer.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalSwapChain::MetalSwapChain(MetalDevice &dev) : device(dev)
    {
    }

    MetalSwapChain::~MetalSwapChain()
    {
        for (auto &slot : slots) {
            ReleaseSlot(slot);
        }
        if (layer != nullptr) {
            (void)(__bridge_transfer CAMetalLayer *)layer;
            layer = nullptr;
        }
    }

    void MetalSwapChain::ReleaseSlot(Slot &slot)
    {
        if (slot.drawable != nullptr) {
            (void)(__bridge_transfer id<CAMetalDrawable>)slot.drawable;
            slot.drawable = nullptr;
        }
        if (slot.image) {
            slot.image->Reset();
        }
    }

    bool MetalSwapChain::Init(const Descriptor &desc)
    {
        if (desc.window == nullptr) {
            LOG_E(TAG, "swapchain requires a CAMetalLayer window pointer");
            return false;
        }

        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for swapchain creation");
            return false;
        }

        auto *metalLayer = (__bridge CAMetalLayer *)desc.window;
        metalLayer.device               = metalDevice;
        metalLayer.pixelFormat          = ToMetalPixelFormat(desc.preferredFormat);
        metalLayer.maximumDrawableCount = IMAGE_COUNT;
        // framebufferOnly=NO so backbuffers can be sampled/copied (RDG reads)
        metalLayer.framebufferOnly = NO;
        if (desc.width != 0 && desc.height != 0) {
            metalLayer.drawableSize = CGSizeMake(desc.width, desc.height);
        } layer = (__bridge_retained void *)metalLayer;
        format = desc.preferredFormat;
        extent = {desc.width, desc.height};

        for (auto &slot : slots) {
            slot.image = std::make_unique<MetalImage>(device);
        }
        return true;
    }

    uint32_t MetalSwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t /*timeoutNs*/)
    {
        const uint32_t index = acquireCursor;
        acquireCursor        = (acquireCursor + 1) % IMAGE_COUNT;

        auto &slot = slots[index];
        ReleaseSlot(slot);

        auto *metalLayer = (__bridge CAMetalLayer *)layer;
        id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
        if (drawable == nil) {
            LOG_E(TAG, "nextDrawable returned nil");
            return INVALID_INDEX;
        }
        slot.drawable = (__bridge_retained void *)drawable;
        slot.image->RebindBorrowed((__bridge void *)drawable.texture);

        // Metal does not provide a native acquire-signal hook. The drawable is
        // immediately CPU-visible; signal the binary semaphore / fence right away
        // so callers can chain wait/Submit safely.
        if (signalSema != nullptr) {
            auto              *sema = static_cast<MetalSemaphore *>(signalSema);
            id<MTLSharedEvent> ev   = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
            const uint64_t     v    = (sema->GetType() == SemaphoreType::TIMELINE) ? 1 : sema->AdvanceBinarySignalValue();
            ev.signaledValue        = v;
        }
        if (fence != nullptr) {
            auto              *f   = static_cast<MetalFence *>(fence);
            const uint64_t     v   = f->TakeNextValue();
            id<MTLSharedEvent> fev = (__bridge id<MTLSharedEvent>)f->GetSharedEvent();
            fev.signaledValue      = v;
        }

        return index;
    }

    void MetalSwapChain::Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas)
    {
        if (imageIndex >= IMAGE_COUNT) {
            return;
        }
        auto &slot = slots[imageIndex];
        if (slot.drawable == nullptr) {
            return;
        }

        id<CAMetalDrawable>  drawable      = (__bridge id<CAMetalDrawable>)slot.drawable;
        auto                *graphicsQueue = (__bridge id<MTLCommandQueue>)device.GetCommandQueue();
        id<MTLCommandBuffer> presentCB     = [graphicsQueue commandBuffer];

        for (uint32_t i = 0; i < numWaitSemas; ++i) {
            auto *sema = static_cast<MetalSemaphore *>(waitSemas[i]);
            if (sema == nullptr)
                continue;
            id<MTLSharedEvent> ev    = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
            const uint64_t     value = (sema->GetType() == SemaphoreType::TIMELINE) ? sema->GetCurrentValue() : sema->GetBinaryWaitValue();
            [presentCB encodeWaitForEvent:ev value:value];
        }

        [presentCB presentDrawable:drawable];
        [presentCB commit];

        ReleaseSlot(slot);
    }

    void MetalSwapChain::Resize(uint32_t width, uint32_t height)
    {
        for (auto &slot : slots) {
            ReleaseSlot(slot);
        }
        extent = {width, height};
        if (layer != nullptr && width != 0 && height != 0) {
            ((__bridge CAMetalLayer *)layer).drawableSize = CGSizeMake(width, height);
        }
    }

    Image *MetalSwapChain::GetImage(uint32_t index) const
    {
        if (index >= IMAGE_COUNT) {
            return nullptr;
        }
        return slots[index].image.get();
    }

    SwapChainStatus MetalSwapChain::GetStatus() const
    {
        auto *metalLayer = (__bridge CAMetalLayer *)layer;
        if (metalLayer == nil) {
            return SwapChainStatus::LOST;
        }
        const Extent2D surfaceSize = GetSurfaceSize();
        if (surfaceSize.width != extent.width || surfaceSize.height != extent.height) {
            return SwapChainStatus::OUT_OF_DATE;
        }
        return SwapChainStatus::OK;
    }

    Extent2D MetalSwapChain::GetSurfaceSize() const
    {
        auto *metalLayer = (__bridge CAMetalLayer *)layer;
        if (metalLayer == nil) {
            return extent;
        }
        const CGSize size = metalLayer.drawableSize;
        return {static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height)};
    }

} // namespace sky::aurora
