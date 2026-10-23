//
// Metal drawable ownership and checked presentation.
//

#include <MetalSwapChain.h>

#include "MetalUtils.h"
#include <MetalDevice.h>
#include <MetalImage.h>
#include <MetalSync.h>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <core/logger/Logger.h>
#include <mutex>
#include <thread>

#import <QuartzCore/CAMetalLayer.h>

namespace sky::aurora {
    namespace {
        GpuError CommandError(id<MTLCommandBuffer> command)
        {
            return {GpuErrorCode::NATIVE_ERROR, command.error ? command.error.code : 0};
        }
    } // namespace

    // nextDrawable has a native one-second timeout, so a single owned worker isolates it from the coordinator.
    struct MetalSwapChain::DrawableRequest {
        ~DrawableRequest()
        {
            {
                std::lock_guard lock(mutex);
                stopping  = true;
                requested = false;
            }
            changed.notify_one();
            if (worker.joinable())
                worker.join();
            ReleaseReady();
        }
        void Start(void *layer)
        {
            worker = std::thread([this, layer]() {
                for (;;) {
                    {
                        std::unique_lock lock(mutex);
                        changed.wait(lock, [this]() { return stopping || requested; });
                        if (stopping)
                            return;
                        requested = false;
                        acquiring = true;
                    }
                    @autoreleasepool {
                        id<CAMetalDrawable> drawable = [(__bridge CAMetalLayer *)layer nextDrawable];
                        std::lock_guard     lock(mutex);
                        acquiring = false;
                        if (!stopping && !discard) {
                            result = (__bridge_retained void *)drawable;
                            ready  = true;
                        }
                        discard = false;
                    }
                }
            });
        }
        bool Take(void *&drawable)
        {
            std::lock_guard lock(mutex);
            if (ready) {
                drawable = result;
                result   = nullptr;
                ready    = false;
                return true;
            }
            if (!requested && !acquiring && !stopping) {
                requested = true;
                changed.notify_one();
            }
            return false;
        }
        bool Cancel()
        {
            std::lock_guard lock(mutex);
            requested = false;
            ReleaseReady();
            discard = acquiring;
            return !acquiring;
        }
        void ReleaseReady()
        {
            if (result)
                (void)(__bridge_transfer id<CAMetalDrawable>)result;
            result = nullptr;
            ready  = false;
        }
        std::mutex              mutex;
        std::condition_variable changed;
        std::thread             worker;
        void                   *result   = nullptr;
        bool                    stopping = false, requested = false, acquiring = false, discard = false, ready = false;
    };

    MetalSwapChain::MetalSwapChain(MetalDevice &dev) : device(dev)
    {
    }

    MetalSwapChain::~MetalSwapChain()
    {
        // Join before dropping the layer/device; a timed-out acquire never proves GPU retirement.
        request.reset();
        if (HasFrames() || (directProtocol && std::any_of(slots.begin(), slots.end(), [](const auto &slot) { return slot.drawable; })))
            device.GetQueue(QueueType::GRAPHICS)->WaitIdle();
        for (auto &slot : slots) {
            if (slot.presentation)
                [(__bridge id<MTLCommandBuffer>)slot.presentation waitUntilCompleted];
            ReleaseSlot(slot);
        }
        if (layer)
            (void)(__bridge_transfer CAMetalLayer *)layer;
    }

    void MetalSwapChain::ReleaseSlot(Slot &slot)
    {
        if (slot.image)
            slot.image->Reset();
        if (slot.drawable)
            (void)(__bridge_transfer id<CAMetalDrawable>)slot.drawable;
        if (slot.presentation)
            (void)(__bridge_transfer id<MTLCommandBuffer>)slot.presentation;
        slot.drawable = slot.presentation = nullptr;
        slot.serial                       = 0;
    }
    bool MetalSwapChain::HasFrames() const noexcept
    {
        return std::any_of(slots.begin(), slots.end(), [](const auto &slot) { return slot.serial != 0; });
    }

    bool MetalSwapChain::Init(const Descriptor &desc)
    {
        if (!desc.window || layer || (desc.preferredFormat != PixelFormat::BGRA8_UNORM && desc.preferredFormat != PixelFormat::BGRA8_SRGB))
            return false;
        auto metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        auto metalLayer  = (__bridge CAMetalLayer *)desc.window;
        if (!metalDevice || ![metalLayer isKindOfClass:[CAMetalLayer class]])
            return false;
        metalLayer.device                    = metalDevice;
        metalLayer.pixelFormat               = ToMetalPixelFormat(desc.preferredFormat);
        metalLayer.maximumDrawableCount      = IMAGE_COUNT;
        metalLayer.framebufferOnly           = NO;
        metalLayer.presentsWithTransaction   = NO;
        metalLayer.allowsNextDrawableTimeout = YES;
        if (desc.width && desc.height)
            metalLayer.drawableSize = CGSizeMake(desc.width, desc.height);
        layer  = (__bridge_retained void *)metalLayer;
        format = desc.preferredFormat;
        extent = {desc.width, desc.height};
        for (auto &slot : slots)
            slot.image = std::make_unique<MetalImage>(device);
        return true;
    }

    Device *MetalSwapChain::GetDevice() const noexcept
    {
        return &device;
    }

    SurfaceQueryResult MetalSwapChain::QuerySurface() const noexcept
    {
        if (closed || !layer)
            return {{PresentationStatus::SURFACE_LOST}, {}};
        auto         metalLayer = (__bridge CAMetalLayer *)layer;
        const CGSize size       = metalLayer.drawableSize;
        if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width > UINT32_MAX || size.height > UINT32_MAX ||
            metalLayer.device != (__bridge id<MTLDevice>)device.GetNativeDevice() || metalLayer.pixelFormat != ToMetalPixelFormat(format))
            return {{PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}}, {}};
        if (size.width <= 0 || size.height <= 0)
            return {{PresentationStatus::BUSY}, {}};
        SurfaceConfiguration configuration{
            {static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height)}, format, SurfaceTransform::IDENTITY, surfaceRevision};
        const bool same = configuration.extent.width == extent.width && configuration.extent.height == extent.height;
        return {{same ? PresentationStatus::READY : PresentationStatus::OUT_OF_DATE}, configuration};
    }

    SwapChainAcquireResult MetalSwapChain::TryAcquireFrame(uint64_t timeoutNs) noexcept
    {
        if (directProtocol || timeoutNs > 1'000'000 || presentationSerial == UINT64_MAX)
            return {{PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}}, {}};
        checkedProtocol    = true;
        const auto surface = QuerySurface();
        if (surface.status != PresentationStatus::READY)
            return {{surface.status, surface.error}, {}};
        auto found = std::find_if(slots.begin(), slots.end(), [](const auto &slot) { return !slot.serial; });
        if (found == slots.end())
            return {{PresentationStatus::BUSY}, {}};
        if (!request) {
            request = std::make_unique<DrawableRequest>();
            request->Start(layer);
        }
        void *drawableHandle = nullptr;
        if (!request->Take(drawableHandle) || !drawableHandle)
            return {{PresentationStatus::BUSY}, {}};
        id<CAMetalDrawable> drawable = (__bridge_transfer id<CAMetalDrawable>)drawableHandle;
        const auto          current  = QuerySurface();
        const auto          texture  = drawable.texture;
        if (current.status != PresentationStatus::READY || texture.width != extent.width || texture.height != extent.height) {
            return {{current.status == PresentationStatus::READY ? PresentationStatus::OUT_OF_DATE : current.status, current.error}, {}};
        }
        auto &slot    = *found;
        slot.drawable = (__bridge_retained void *)drawable;
        slot.image->RebindBorrowed((__bridge void *)texture);
        slot.serial = ++presentationSerial;
        return {{PresentationStatus::READY}, {slot.serial, static_cast<uint32_t>(found - slots.begin())}};
    }

    PresentationResult MetalSwapChain::TryPresentFrame(uint64_t serial, bool gpuComplete) noexcept
    {
        @autoreleasepool {
            auto found = std::find_if(slots.begin(), slots.end(), [serial](const auto &slot) { return serial && slot.serial == serial; });
            if (directProtocol || found == slots.end() || found->presentation)
                return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
            if (!gpuComplete)
                return {PresentationStatus::BUSY};
            auto                 queue   = (__bridge id<MTLCommandQueue>)device.GetCommandQueue();
            id<MTLCommandBuffer> command = [queue commandBuffer];
            if (!command)
                return {PresentationStatus::FAILED, {GpuErrorCode::OUT_OF_MEMORY}};
            [command presentDrawable:(__bridge id<CAMetalDrawable>)found->drawable];
            found->presentation = (__bridge_retained void *)command;
            [command commit];
            if (command.status == MTLCommandBufferStatusError)
                return {PresentationStatus::FAILED, CommandError(command)};
            return {PresentationStatus::READY};
        }
    }

    FenceQueryResult MetalSwapChain::RetireFrame(uint64_t serial, bool abandon) noexcept
    {
        auto found = std::find_if(slots.begin(), slots.end(), [serial](const auto &slot) { return serial && slot.serial == serial; });
        if (directProtocol || found == slots.end() || (found->presentation != nullptr) == abandon)
            return {FenceStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        if (!abandon) {
            auto command = (__bridge id<MTLCommandBuffer>)found->presentation;
            if (command.status == MTLCommandBufferStatusError)
                return {FenceStatus::FAILED, CommandError(command)};
            if (command.status != MTLCommandBufferStatusCompleted)
                return {FenceStatus::PENDING};
        }
        // Completion retires our native references; it does not report physical display time.
        ReleaseSlot(*found);
        return {FenceStatus::SIGNALED};
    }

    PresentationResult MetalSwapChain::TryResize(uint32_t width, uint32_t height) noexcept
    {
        if (directProtocol || surfaceRevision == UINT64_MAX)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        checkedProtocol = true;
        if (closed || !layer)
            return {PresentationStatus::SURFACE_LOST};
        if (!width || !height || HasFrames() || (request && !request->Cancel()))
            return {PresentationStatus::BUSY};
        ((__bridge CAMetalLayer *)layer).drawableSize = CGSizeMake(width, height);
        extent                                        = {width, height};
        ++surfaceRevision;
        return {PresentationStatus::READY};
    }
    PresentationResult MetalSwapChain::TryClose() noexcept
    {
        if (directProtocol)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        checkedProtocol = true;
        if (HasFrames() || (request && !request->Cancel()))
            return {PresentationStatus::BUSY};
        closed = true;
        return {PresentationStatus::READY};
    }

    uint32_t MetalSwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t)
    {
        if (checkedProtocol || closed || (signalSema && signalSema->GetDevice() != &device) || (fence && fence->GetDevice() != &device))
            return INVALID_INDEX;
        directProtocol             = true;
        auto          *nativeFence = fence ? static_cast<MetalFence *>(fence) : nullptr;
        const uint64_t fenceValue  = nativeFence ? nativeFence->TakeNextValue() : 0;
        if (nativeFence && !fenceValue)
            return INVALID_INDEX;
        const uint32_t index = acquireCursor;
        acquireCursor        = (acquireCursor + 1) % IMAGE_COUNT;
        auto &slot           = slots[index];
        ReleaseSlot(slot);
        id<CAMetalDrawable> drawable = [(__bridge CAMetalLayer *)layer nextDrawable];
        if (!drawable)
            return INVALID_INDEX;
        slot.drawable = (__bridge_retained void *)drawable;
        slot.image->RebindBorrowed((__bridge void *)drawable.texture);
        if (signalSema) {
            auto *sema          = static_cast<MetalSemaphore *>(signalSema);
            auto  event         = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
            event.signaledValue = sema->GetType() == SemaphoreType::TIMELINE ? 1 : sema->AdvanceBinarySignalValue();
        }
        if (nativeFence) {
            auto event          = (__bridge id<MTLSharedEvent>)nativeFence->GetSharedEvent();
            event.signaledValue = fenceValue;
        }
        return index;
    }
    void MetalSwapChain::Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas)
    {
        if (checkedProtocol || closed || imageIndex >= IMAGE_COUNT || !slots[imageIndex].drawable || (numWaitSemas && !waitSemas))
            return;
        for (uint32_t i = 0; i < numWaitSemas; ++i)
            if (!waitSemas[i] || waitSemas[i]->GetDevice() != &device)
                return;
        directProtocol               = true;
        auto                 queue   = (__bridge id<MTLCommandQueue>)device.GetCommandQueue();
        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (!command)
            return;
        for (uint32_t i = 0; i < numWaitSemas; ++i) {
            auto *sema  = static_cast<MetalSemaphore *>(waitSemas[i]);
            auto  event = (__bridge id<MTLSharedEvent>)sema->GetSharedEvent();
            [command encodeWaitForEvent:event
                                  value:sema->GetType() == SemaphoreType::TIMELINE ? sema->GetCurrentValue() : sema->GetBinaryWaitValue()];
        }
        [command presentDrawable:(__bridge id<CAMetalDrawable>)slots[imageIndex].drawable];
        [command commit];
        ReleaseSlot(slots[imageIndex]);
    }
    void MetalSwapChain::Resize(uint32_t width, uint32_t height)
    {
        if (checkedProtocol || closed)
            return;
        directProtocol = true;
        for (auto &slot : slots)
            ReleaseSlot(slot);
        extent = {width, height};
        if (layer && width && height)
            ((__bridge CAMetalLayer *)layer).drawableSize = CGSizeMake(width, height);
    }
    Image *MetalSwapChain::GetImage(uint32_t index) const
    {
        return index < IMAGE_COUNT ? slots[index].image.get() : nullptr;
    }
    SwapChainStatus MetalSwapChain::GetStatus() const
    {
        const auto surface = QuerySurface();
        return surface.status == PresentationStatus::SURFACE_LOST || surface.status == PresentationStatus::FAILED ? SwapChainStatus::LOST
               : surface.status == PresentationStatus::OUT_OF_DATE                                                ? SwapChainStatus::OUT_OF_DATE
                                                                                                                  : SwapChainStatus::OK;
    }
    Extent2D MetalSwapChain::GetSurfaceSize() const
    {
        const auto surface = QuerySurface();
        return surface.status == PresentationStatus::READY || surface.status == PresentationStatus::OUT_OF_DATE ? surface.configuration.extent
                                                                                                                : Extent2D{};
    }
} // namespace sky::aurora
