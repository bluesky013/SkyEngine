//
// Created on 2026/04/02.
//

#include <MetalSwapChain.h>
#include <MetalDevice.h>
#include <MetalImage.h>
#include <MetalSync.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalSwapChain::MetalSwapChain(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalSwapChain::~MetalSwapChain()
    {
        ReleaseDrawable();
        image.reset();
        if (layer != nullptr) {
            [(CAMetalLayer *)layer release];
            layer = nullptr;
        }
    }

    void MetalSwapChain::ReleaseDrawable()
    {
        if (currentDrawable != nullptr) {
            [(id<CAMetalDrawable>)currentDrawable release];
            currentDrawable = nullptr;
        }
        if (image) {
            image->Reset();
        }
    }

    bool MetalSwapChain::Init(const Descriptor &desc)
    {
        if (desc.window == nullptr) {
            LOG_E(TAG, "swapchain requires a CAMetalLayer window pointer");
            return false;
        }

        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        if (metalDevice == nil) {
            LOG_E(TAG, "invalid Metal device for swapchain creation");
            return false;
        }

        auto *metalLayer = (CAMetalLayer *)desc.window;
        [metalLayer retain];
        metalLayer.device          = metalDevice;
        metalLayer.pixelFormat     = ToMetalPixelFormat(desc.preferredFormat);
        metalLayer.framebufferOnly = YES;
        if (desc.width != 0 && desc.height != 0) {
            metalLayer.drawableSize = CGSizeMake(desc.width, desc.height);
        }

        layer  = metalLayer;
        format = desc.preferredFormat;
        extent = {desc.width, desc.height};

        image = std::make_unique<MetalImage>(device);
        return true;
    }

    uint32_t MetalSwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t /*timeoutNs*/)
    {
        ReleaseDrawable();

        auto *metalLayer = (CAMetalLayer *)layer;
        id<CAMetalDrawable> drawable = [[metalLayer nextDrawable] retain];
        if (drawable == nil) {
            LOG_E(TAG, "nextDrawable returned nil");
            return INVALID_INDEX;
        }
        currentDrawable = drawable;
        image->RebindBorrowed(drawable.texture);

        // Metal does not provide a native acquire-signal hook. The drawable is
        // immediately CPU-visible; signal the binary semaphore / fence right away
        // so callers can chain wait/Submit safely.
        if (signalSema != nullptr) {
            auto *sema = static_cast<MetalSemaphore *>(signalSema);
            id<MTLSharedEvent> ev = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
            const uint64_t v = (sema->GetType() == SemaphoreType::TIMELINE)
                                   ? 1
                                   : sema->AdvanceBinarySignalValue();
            ev.signaledValue = v;
        }
        if (fence != nullptr) {
            auto *f = static_cast<MetalFence *>(fence);
            const uint64_t v = f->TakeNextValue();
            id<MTLSharedEvent> fev = (__bridge id<MTLSharedEvent>)f->GetSharedEvent();
            fev.signaledValue = v;
        }

        return 0;
    }

    void MetalSwapChain::Present(uint32_t /*imageIndex*/, uint32_t numWaitSemas, Semaphore *const *waitSemas)
    {
        if (currentDrawable == nullptr) {
            return;
        }

        id<CAMetalDrawable> drawable = (id<CAMetalDrawable>)currentDrawable;
        auto *graphicsQueue = (id<MTLCommandQueue>)device.GetCommandQueue();
        id<MTLCommandBuffer> presentCB = [graphicsQueue commandBuffer];

        for (uint32_t i = 0; i < numWaitSemas; ++i) {
            auto *sema = static_cast<MetalSemaphore *>(waitSemas[i]);
            if (sema == nullptr) continue;
            id<MTLSharedEvent> ev = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
            const uint64_t value = (sema->GetType() == SemaphoreType::TIMELINE)
                                       ? sema->GetCurrentValue()
                                       : sema->GetBinaryWaitValue();
            [presentCB encodeWaitForEvent:ev value:value];
        }

        [presentCB presentDrawable:drawable];
        [presentCB commit];

        ReleaseDrawable();
    }

    void MetalSwapChain::Resize(uint32_t width, uint32_t height)
    {
        ReleaseDrawable();
        extent = {width, height};
        if (layer != nullptr && width != 0 && height != 0) {
            ((CAMetalLayer *)layer).drawableSize = CGSizeMake(width, height);
        }
    }

    Image *MetalSwapChain::GetImage(uint32_t index) const
    {
        if (index != 0) {
            return nullptr;
        }
        return image.get();
    }

} // namespace sky::aurora
