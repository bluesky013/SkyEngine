#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/SubmitInfo.h>
#include <cstddef>
#include <span>

namespace sky::aurora {

    struct FrameSubmissionSync {
        std::vector<SemaphoreSubmitInfo> waits;
        std::vector<SemaphoreSubmitInfo> signals;
        std::shared_ptr<const void>      owner;
    };

    class Fence;
    class FrameResourceTracking;
    using FrameResourceTrackingResult = std::variant<FrameError, std::shared_ptr<const FrameResourceTracking>>;
    using FrameResource               = std::variant<BufferPtr, ImagePtr>;
    using FrameResourceBoundaryState  = std::variant<CopyBufferState, ImageBoundaryState>;
    struct FrameResourceState {
        FrameResourceBoundaryState state;
        SubmissionToken            submission;
    };
    struct FrameResourceStateSnapshot {
        FrameResourceState accepted;
        FrameResourceState completed;
    };
    using FrameResourceStateResult = std::variant<FrameError, FrameResourceStateSnapshot>;

    enum class VulkanFrameGraphMode : uint8_t { DYNAMIC_RENDERING, RENDER_PASS };

    struct FrameAllocationLimits {
        uint64_t stagingBytes        = 4 * 1024 * 1024;
        uint64_t uniformBytes        = 1024 * 1024;
        uint64_t scratchBytes        = 16 * 1024 * 1024;
        uint32_t resourceDescriptors = 1024;
        uint32_t samplerDescriptors  = 128;
        uint32_t descriptorSets      = 256;
        uint32_t renderTargets       = 128;
        uint64_t storageBytes = 4 * 1024 * 1024;
    };
    enum class FrameBufferKind : uint8_t { STAGING, UNIFORM, STORAGE = 3 };
    // Uniform size includes zero-filled binding padding; ownership does not extend lease validity.
    struct FrameBufferSlice {
        DeviceFrameLease lease;
        BufferPtr        buffer;
        uint64_t         offset = 0;
        uint64_t         size   = 0;
    };
    using FrameBufferAllocation = std::variant<FrameError, FrameBufferSlice>;

    struct DeviceFrameContextInitInfo {
        uint32_t              inflightNum          = 2;
        uint32_t              parallelNum          = 1;
        uint64_t              deviceEpoch          = 1;
        uint32_t              maxRecordingsPerSlot = 256;
        VulkanFrameGraphMode  vulkanCompiler       = VulkanFrameGraphMode::DYNAMIC_RENDERING;
        FrameAllocationLimits allocations;
        uint32_t              trackedResources         = 1024;
        uint32_t              trackedImageSubresources = MAX_FRAME_IMAGE_SUBRESOURCES;
    };

    // The coordinator serializes calls; Device must outlive its context, slices, plans and recordings.
    class DeviceFrameContext {
    public:
        virtual ~DeviceFrameContext()                             = default;
        virtual FrameContextIdentity GetIdentity() const noexcept = 0;
        // No fence reset or implied submission. Busy never consumes a generation.
        virtual BeginSlotResult TryBeginSlot() noexcept = 0;
        // Read-only capacity after alignment; valid only while the lease accepts allocations.
        virtual std::variant<FrameError, uint64_t>
        AvailableFrameBufferBytes(const DeviceFrameLease &, FrameBufferKind, uint64_t alignment = 1) const = 0;
        // Copies owned input before Compile; slices are usable only within their lease, never after retirement.
        virtual FrameBufferAllocation
        WriteFrameBuffer(const DeviceFrameLease &, FrameBufferKind, std::span<const std::byte>, uint64_t alignment = 1) = 0;
        virtual FrameCompileResult   Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>)         = 0;
        virtual FrameDispatchResult  Dispatch(std::unique_ptr<BackendCompiledFrame> &)                                  = 0;
        virtual FrameCollectResult   TryCollect(const EncodingTicket &)                                                 = 0;
        virtual FrameOperationResult Cancel(const EncodingTicket &)                                                     = 0;
        // Rejected preserves the recording; Accepted/Indeterminate consume it and retain GPU references.
        virtual FrameSubmitResult    Submit(std::unique_ptr<RecordedFrame> &) noexcept     = 0;
        virtual FrameSubmitResult    Submit(std::unique_ptr<RecordedFrame> &, const FrameSubmissionSync &) noexcept
        {
            return SubmissionRejected{FrameError{FrameErrorCode::UNSUPPORTED}};
        }
        virtual FrameOperationResult DiscardUnsubmitted(const DeviceFrameLease &) noexcept = 0;
        // Deliver once. GPU completion alone does not retire presentation.
        virtual size_t PollCompleted(std::span<FrameCompletion>) noexcept = 0;

        // Explicit ownership of buffer and per-subresource image boundary states on this context's graphics queue; no content tracking.
        virtual FrameOperationResult TrackResource(FrameResource, FrameResourceBoundaryState)
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        // Dropping the last owner requests retirement; only the coordinator collects after all GPU/plan uses retire.
        virtual FrameResourceTrackingResult TrackOwnedResource(FrameResource, FrameResourceBoundaryState)
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        virtual FrameResourceStateResult GetResourceState(const FrameResource &) const
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        // Releases the identity pin only after the latest accepted use completes and no plan borrows it.
        virtual FrameOperationResult UntrackResource(const FrameResource &)
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }

        // Direct hosts remain separate until migration; a context cannot mix the two protocols.
        virtual void   BeginFrame() noexcept          = 0;
        virtual void   EndFrame() noexcept            = 0;
        virtual Fence *GetFrameFence() const noexcept = 0;
    };

} // namespace sky::aurora
