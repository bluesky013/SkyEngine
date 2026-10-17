//
// Created on 2026/10/17.
//

#pragma once

#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/renderer/RendererTypes.h>
#include <aurora/rhi/SwapChain.h>
#include <core/util/Uuid.h>

namespace sky::aurora {
    class ResourceUploader;

    struct OutputSnapshot {
        ViewportId       viewport;
        uint64_t         revision = 0;
        Extent2D         extent{};
        SurfaceTransform transform       = SurfaceTransform::IDENTITY;
        uint64_t         surfaceRevision = 0;
        PixelFormat      format          = PixelFormat::UNDEFINED;
    };
    enum class OutputProgressKind : uint8_t { SKIPPED, FAILED, CLOSED };
    enum class OutputFailureDisposition : uint8_t { RETRY, REPLACE, SHUTDOWN };
    struct OutputProgress {
        ViewportId               viewport;
        OutputProgressKind       kind = OutputProgressKind::SKIPPED;
        GpuError                 error;
        OutputFailureDisposition disposition = OutputFailureDisposition::RETRY;
    };
    struct AcquiredOutput {
        OutputSnapshot  snapshot;
        FrameGraphImage image;
    };
    struct OutputAcquireBatch {
        std::vector<AcquiredOutput> outputs;
        std::vector<OutputProgress> skipped;
    };

    // Coordinator-owned output records; GPU slots and completion polling stay with their existing owner.
    class FrameScheduler final {
    public:
        FrameScheduler(Device &, DeviceFrameContext &);
        ~FrameScheduler();
        FrameOperationResult                         AddOutput(ViewportId, SwapChainPtr);
        FrameOperationResult                         RequestResize(ViewportId, Extent2D);
        FrameOperationResult                         RequestClose(ViewportId);
        void                                         RequestShutdown();
        std::variant<OutputSnapshot, OutputProgress> BeginViewport(ViewportId);
        std::vector<OutputSnapshot>                  Snapshot() const;
        std::variant<FrameError, OutputAcquireBatch> Acquire(const DeviceFrameLease &, std::span<const OutputSnapshot>);
        FrameSubmitResult                            Submit(std::unique_ptr<RecordedFrame> &, ResourceUploader * = nullptr, const Uuid &batch = {});
        void                                         Abandon(const DeviceFrameLease &);
        void                                         AbandonOutput(const DeviceFrameLease &, ViewportId);
        void                                         Retain(const DeviceFrameLease &, std::shared_ptr<const void>);
        void                                         OnCompleted(std::span<const FrameCompletion>);
        size_t                                       Pump(std::span<OutputProgress>);
        bool                                         Empty() const noexcept;

    private:
        struct State;
        std::unique_ptr<State> state;
    };

} // namespace sky::aurora
