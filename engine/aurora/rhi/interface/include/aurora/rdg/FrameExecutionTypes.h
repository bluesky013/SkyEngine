#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/SubmissionResult.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sky::aurora {

    // IDs are issued/validated by the owning context, not host frame counters.
    struct FrameContextIdentity {
        API      backend                                        = API::DEFAULT;
        uint64_t deviceEpoch                                    = 0;
        uint64_t contextId                                      = 0;
        bool     operator==(const FrameContextIdentity &) const = default;
    };

    struct DeviceFrameLease {
        FrameContextIdentity context;
        uint32_t             slot                                       = std::numeric_limits<uint32_t>::max();
        uint64_t             generation                                 = 0;
        bool                 operator==(const DeviceFrameLease &) const = default;
    };

    struct CompiledFrameIdentity {
        DeviceFrameLease lease;
        uint64_t         graphId                                         = 0;
        uint64_t         planId                                          = 0;
        bool             operator==(const CompiledFrameIdentity &) const = default;
    };

    struct EncodingTicket {
        CompiledFrameIdentity frame;
        uint64_t              sessionId                                = 0;
        bool                  operator==(const EncodingTicket &) const = default;
    };

    struct SubmissionToken {
        DeviceFrameLease lease;
        uint64_t         serial                                    = 0;
        bool             operator==(const SubmissionToken &) const = default;
    };

    enum class FrameErrorCode : uint8_t {
        UNSUPPORTED,
        INVALID_ARGUMENT,
        WRONG_CONTEXT,
        STALE_LEASE,
        INVALID_STATE,
        OUT_OF_MEMORY,
        COMPILE_FAILED,
        ENCODE_FAILED,
        SUBMIT_REJECTED,
        SUBMIT_UNCERTAIN,
        DEVICE_LOST
    };

    struct FrameError {
        FrameErrorCode code = FrameErrorCode::INVALID_STATE;
        std::string    message;
        GpuError       cause;
    };

    struct FrameBusy {};
    struct FramePending {};
    struct FrameOperationComplete {};
    using FrameOperationResult = std::variant<FrameError, FrameOperationComplete>;
    using BeginSlotResult      = std::variant<FrameError, FrameBusy, DeviceFrameLease>;

    // Native passes, bindings, CPU encode tasks and GPU submission order remain backend-owned.
    class BackendCompiledFrame {
    public:
        virtual ~BackendCompiledFrame()                            = default;
        virtual CompiledFrameIdentity GetIdentity() const noexcept = 0;
    };

    class FrozenFrameGraph;
    struct FrameGraphReceipt {
        std::shared_ptr<const FrozenFrameGraph> graph;
        std::vector<uint32_t>                   passes;
    };
    class RecordedFrame {
    public:
        virtual ~RecordedFrame()                                   = default;
        virtual CompiledFrameIdentity GetIdentity() const noexcept = 0;
        // Issued only after every live pass and final transition has been recorded successfully.
        virtual const FrameGraphReceipt *GetGraphReceipt() const noexcept
        {
            return nullptr;
        }
    };

    struct FrameNoWork {};
    using FrameCompileResult  = std::variant<FrameError, FrameNoWork, std::unique_ptr<BackendCompiledFrame>>;
    using FrameDispatchResult = std::variant<FrameError, EncodingTicket>;
    struct EncodingFailed {
        FrameError error;
    }; // all kernels/borrowers drained
    struct EncodingCancelled {}; // all kernels/borrowers drained
    using FrameCollectResult = std::variant<FrameError, FramePending, EncodingFailed, EncodingCancelled, std::unique_ptr<RecordedFrame>>;

    struct SubmissionAccepted {
        SubmissionToken token;
    };
    struct SubmissionRejected {
        FrameError error;
    }; // definitely no GPU enqueue
    struct SubmissionIndeterminate {
        SubmissionToken token;
        FrameError      error;
    };
    using FrameSubmitResult = std::variant<SubmissionRejected, SubmissionAccepted, SubmissionIndeterminate>;

    enum class FrameCompletionStatus : uint8_t { COMPLETE, DEVICE_LOST, FAILED };
    struct FrameCompletion {
        SubmissionToken       token;
        FrameCompletionStatus status = FrameCompletionStatus::FAILED;
        FrameError            error; // meaningful unless COMPLETE
    };

} // namespace sky::aurora
