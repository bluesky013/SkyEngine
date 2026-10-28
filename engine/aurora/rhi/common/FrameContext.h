#pragma once

#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/DirectFrameContext.h>
#include <aurora/rhi/Device.h>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace sky::aurora {

    // Read results only after all workers have completed; preserve the first error at the highest priority.
    FrameError *FindEncodingFailure(std::span<std::optional<FrameError>> errors) noexcept;

    struct FrameRecordingInputs {
        FrameGraphReceipt                  receipt;
        std::vector<CommandBuffer *>       commands;
        std::vector<SemaphoreSubmitInfo>   waits;
        std::vector<SemaphoreSubmitInfo>   signals;
        std::vector<CounterPtr<RefObject>> resources;
        std::shared_ptr<const void>        payload;
    };
    using CommandAllocationResult = std::variant<FrameError, CommandBuffer *>;
    using FrameRecordResult       = std::variant<FrameError, std::unique_ptr<RecordedFrame>>;

    struct FrameBufferStorage {
        BufferPtr buffer;
        uint8_t  *mapped = nullptr;
    };
    using FrameBufferStorageResult = std::variant<FrameError, FrameBufferStorage>;
    // Checked linear ranges have no frame counter; the owner resets them only after lease retirement.
    std::optional<uint64_t> FrameRangeOffset(uint64_t cursor, uint64_t capacity, uint64_t size, uint64_t alignment) noexcept;

    struct FrameResourceBindings;
    struct FrameStateChanges;
    struct FramePlanLease {
        CompiledFrameIdentity                    identity;
        std::shared_ptr<const uint8_t>           borrower;
        std::shared_ptr<const FrameStateChanges> states;
    };

    // Backend-internal slot storage; compilation and native encoding policy remain in each backend.
    class BackendFrameContext : public DeviceFrameContext, public DirectFrameContext {
    public:
        BackendFrameContext();
        ~BackendFrameContext() override;
        BackendFrameContext(const BackendFrameContext &)            = delete;
        BackendFrameContext &operator=(const BackendFrameContext &) = delete;

        template <typename T, typename D>
        static DeviceFrameContext *Create(D *device, const DeviceFrameContextInitInfo &info) noexcept
        {

            auto context = std::make_unique<T>(device);
            return context->Init(device, info) ? context.release() : nullptr;
        }

        template <typename T, typename D>
        static DirectFrameContext *CreateDirect(D *device, const DeviceFrameContextInitInfo &info) noexcept
        {
            auto context = std::make_unique<T>(device);
            return context->Init(device, info, true) ? context.release() : nullptr;
        }

        FrameContextIdentity GetIdentity() const noexcept override;
        BeginSlotResult      TryBeginSlot() noexcept override;
        std::variant<FrameError, uint64_t>
        AvailableFrameBufferBytes(const DeviceFrameLease &, FrameBufferKind, uint64_t alignment = 1) const override;
        FrameBufferAllocation
                             WriteFrameBuffer(const DeviceFrameLease &, FrameBufferKind, std::span<const std::byte>, uint64_t alignment = 1) override;
        FrameCompileResult   Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>) override;
        FrameDispatchResult  Dispatch(std::unique_ptr<BackendCompiledFrame> &) override;
        FrameCollectResult   TryCollect(const EncodingTicket &) override;
        FrameOperationResult Cancel(const EncodingTicket &) override;
        FrameSubmitResult    Submit(std::unique_ptr<RecordedFrame> &) noexcept override;
        FrameSubmitResult    Submit(std::unique_ptr<RecordedFrame> &, const FrameSubmissionSync &) noexcept override;
        FrameOperationResult DiscardUnsubmitted(const DeviceFrameLease &) noexcept override;
        size_t               PollCompleted(std::span<FrameCompletion>) noexcept override;

        FrameOperationResult        TrackResource(FrameResource, FrameResourceBoundaryState) override;
        FrameResourceTrackingResult TrackOwnedResource(FrameResource, FrameResourceBoundaryState) override;
        FrameResourceStateResult    GetResourceState(const FrameResource &) const override;
        FrameOperationResult        UntrackResource(const FrameResource &) override;

        // Internal hand-off from the backend encoder; ordinals are unique within the active lease.
        virtual CommandAllocationResult AllocateCommand(const DeviceFrameLease &, uint32_t ordinal) noexcept;
        virtual FrameRecordResult       FinishRecording(const DeviceFrameLease &, FrameRecordingInputs inputs) noexcept;

        DirectFrameBeginResult TryBeginFrame() noexcept override;
        FrameOperationResult   EndFrame() noexcept override;
        Fence *GetFrameFence() const noexcept override;

    protected:
        virtual bool InitBackend(const DeviceFrameContextInitInfo &)
        {
            return true;
        }
        virtual FrameOperationResult ResetLeaseResources(uint32_t) noexcept
        {
            return FrameOperationComplete{};
        }
        virtual FrameBufferStorageResult CreateFrameBuffer(const Buffer::Descriptor &)
        {
            return FrameError{FrameErrorCode::UNSUPPORTED};
        }
        FrameBufferAllocation AllocateScratch(const DeviceFrameLease &, uint64_t size, uint64_t alignment);
        uint64_t              GetScratchCursor(const DeviceFrameLease &) const noexcept;
        void                  RestoreScratchCursor(const DeviceFrameLease &, uint64_t) noexcept;
        virtual void          SelectDirectSlot(uint32_t) noexcept
        {
        }
        FrameOperationResult ValidateCompileLease(const DeviceFrameLease &) const noexcept;
        std::variant<FrameError, FramePlanLease>
                                ReservePlan(const DeviceFrameLease &, const FrozenFrameGraph &, const FrameResourceBindings &) noexcept;
        FrameOperationResult    ValidatePlan(const FramePlanLease &) const noexcept;
        FrameDispatchResult     BeginEncoding(const FramePlanLease &) noexcept;
        CommandAllocationResult AllocatePlanCommand(const FramePlanLease &, uint32_t ordinal) noexcept;
        FrameRecordResult       FinishPlanRecording(const FramePlanLease &, FrameRecordingInputs) noexcept;
        void                    PublishEncoding(const EncodingTicket &, FrameRecordResult) noexcept;
        void                    PublishCancelledEncoding(const EncodingTicket &) noexcept;
        void                    ObserveFailure(const FrameError &) noexcept;

    private:
        FrameBufferAllocation   AllocateBuffer(const DeviceFrameLease &, uint32_t kind, uint64_t size, uint64_t alignment);
        bool                    Init(Device *, const DeviceFrameContextInitInfo &, bool direct = false);
        CommandAllocationResult Allocate(const DeviceFrameLease &, uint32_t ordinal) noexcept;
        FrameRecordResult       Record(const DeviceFrameLease &, FrameRecordingInputs, const FramePlanLease *) noexcept;
        struct State;
        std::unique_ptr<State> state;
    };

} // namespace sky::aurora
