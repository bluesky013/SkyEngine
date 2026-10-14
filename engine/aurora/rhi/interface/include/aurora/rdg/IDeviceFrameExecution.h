#pragma once

#include <aurora/rdg/IFrameGraphCompiler.h>
#include <span>

namespace sky::aurora {

    // Coordinator-only; validate backend/context/device epoch, slot generation and single use before accessing resources.
    // CPU encoding dependencies do not imply GPU synchronization.
    class IDeviceFrameExecution {
    public:
        virtual ~IDeviceFrameExecution()                          = default;
        virtual FrameContextIdentity GetIdentity() const noexcept = 0;

        // Nonblocking, at most one Recording lease; reset allocations only in GPU-retired slots.
        // Acquiring a slot must not reset its fence or create a submission wait.
        virtual BeginSlotResult TryBeginSlot() = 0;
        // Uses this context's backend compiler.
        virtual FrameCompileResult Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph) = 0;
        // Accepted dispatch consumes/clears plan and pins borrowers; preflight errors preserve plan.
        virtual FrameDispatchResult Dispatch(std::unique_ptr<BackendCompiledFrame> &plan) = 0;
        // Deliver terminal results once, after workers and coordinator tasks drain.
        // Invalid tickets yield FrameError; encoding failures yield EncodingFailed.
        virtual FrameCollectResult TryCollect(const EncodingTicket &ticket) = 0;
        // Request only; idempotent for a live ticket. TryCollect still must drain.
        virtual FrameOperationResult Cancel(const EncodingTicket &ticket) = 0;

        // Accepted/indeterminate consume frame, transfer retention to context and invalidate the recording lease.
        // Rejected preserves frame for discard; indeterminate quarantines the slot until completion/device teardown.
        virtual FrameSubmitResult Submit(std::unique_ptr<RecordedFrame> &frame) = 0;
        // Unsubmitted only: destroy plans/recordings, drain encoders and resolve acquired outputs before discard.
        // Presentation retirement remains the output owner's responsibility.
        virtual FrameOperationResult DiscardUnsubmitted(const DeviceFrameLease &lease) = 0;
        // Deliver terminal records once; GPU completion does not retire presentation.
        // A device error cannot turn an uncertain submit into a definite rejection.
        virtual size_t PollCompleted(std::span<FrameCompletion> destination) = 0;
    };

} // namespace sky::aurora
