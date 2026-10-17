//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/SubmissionResult.h>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    class Image;
    class Semaphore;
    class Fence;
    class Device;

    enum class PresentationStatus : uint8_t { READY, BUSY, OUT_OF_DATE, SURFACE_LOST, FAILED };
    struct PresentationResult {
        PresentationStatus status = PresentationStatus::FAILED;
        GpuError           error;
    };
    struct AcquiredSwapChainFrame {
        uint64_t   serial     = 0;
        uint32_t   imageIndex = INVALID_INDEX;
        Semaphore *acquired   = nullptr;
        Semaphore *rendered   = nullptr;
    };
    struct SwapChainAcquireResult : PresentationResult {
        AcquiredSwapChainFrame frame;
    };

    enum class SurfaceTransform : uint8_t {
        IDENTITY,
        ROTATE_90,
        ROTATE_180,
        ROTATE_270,
        MIRROR,
        MIRROR_ROTATE_90,
        MIRROR_ROTATE_180,
        MIRROR_ROTATE_270
    };
    struct SurfaceConfiguration {
        Extent2D         extent{};
        PixelFormat      format    = PixelFormat::UNDEFINED;
        SurfaceTransform transform = SurfaceTransform::IDENTITY;
        uint64_t         revision  = 0;
    };
    struct SurfaceQueryResult : PresentationResult {
        SurfaceConfiguration configuration;
    };

    enum class SwapChainStatus : uint32_t {
        OK          = 0,
        OUT_OF_DATE = 1,   // surface size changed, needs Resize/recreate
        LOST        = 2,   // surface lost, unrecoverable this frame
    };

    class SwapChain : public RefObject {
    public:
        struct Descriptor {
            void       *window          = nullptr;
            uint32_t    width           = 1;
            uint32_t    height          = 1;
            PixelFormat preferredFormat = PixelFormat::BGRA8_UNORM;
            PresentMode preferredMode   = PresentMode::IMMEDIATE;
        };

        SwapChain() = default;
        ~SwapChain() override = default;

        virtual Device *GetDevice() const noexcept
        {
            return nullptr;
        }
        // READY describes the applied configuration; OUT_OF_DATE describes the required replacement.
        virtual SurfaceQueryResult QuerySurface() const noexcept
        {
            return {{PresentationStatus::FAILED, {GpuErrorCode::UNSUPPORTED}}, {}};
        }
        // Checked frames exclude the direct host protocol; caller serializes queue access. Timeout is at most 1 ms.
        virtual SwapChainAcquireResult TryAcquireFrame(uint64_t) noexcept
        {
            return {{PresentationStatus::FAILED, {GpuErrorCode::UNSUPPORTED}}, {}};
        }
        virtual PresentationResult TryPresentFrame(uint64_t, bool) noexcept
        {
            return {PresentationStatus::FAILED, {GpuErrorCode::UNSUPPORTED}};
        }
        // Call after GPU completion and CPU borrowers exit; abandon means no present was enqueued.
        virtual FenceQueryResult RetireFrame(uint64_t, bool) noexcept
        {
            return {FenceStatus::FAILED, {GpuErrorCode::UNSUPPORTED}};
        }
        virtual PresentationResult TryResize(uint32_t, uint32_t) noexcept
        {
            return {PresentationStatus::FAILED, {GpuErrorCode::UNSUPPORTED}};
        }
        virtual PresentationResult TryClose() noexcept
        {
            return {PresentationStatus::FAILED, {GpuErrorCode::UNSUPPORTED}};
        }

        // Acquire the next backbuffer index. signalSema (binary) and/or fence are
        // signaled when the image is actually available.
        // Returns INVALID_INDEX on timeout / out-of-date swapchain.
        virtual uint32_t AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) = 0;

        // Present the given image. waitSemas must all be binary semaphores.
        virtual void Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) = 0;

        // Recreate the swapchain at the new size. All Image* returned by GetImage
        // become invalid.
        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual Image*      GetImage(uint32_t index) const = 0;
        virtual uint32_t    GetImageCount() const = 0;
        virtual PixelFormat GetFormat() const = 0;
        virtual Extent2D    GetExtent() const = 0;

        // Self-check the surface state. The target size is owned by the native
        // window, so the backend queries it (surface caps / layer size) and
        // compares against the current extent. SUBOPTIMAL is treated as OK.
        virtual SwapChainStatus GetStatus() const = 0;

        // Current native-window size, self-queried by the backend (surface caps /
        // layer size). Used to drive Resize() when GetStatus() returns OUT_OF_DATE.
        virtual Extent2D GetSurfaceSize() const = 0;
    };

    using SwapChainPtr = CounterPtr<SwapChain>;

} // namespace sky::aurora
