#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace sky {

    enum class CompletionState : uint8_t { PENDING, SUCCEEDED, FAILED, CANCELLED };

    struct CompletionResult {
        CompletionState state = CompletionState::PENDING;
        std::string     diagnostic;
    };

    struct CompletionSnapshot {
        CompletionResult result;
        bool             cancelRequested = false; // request is NOT a terminal/drained state
    };

    // Owner-issued; zero fields are invalid, generation rejects subscriptions to a replaced executor.
    struct ContinuationExecutorId {
        uint64_t id                                               = 0;
        uint64_t generation                                       = 0;
        bool     operator==(const ContinuationExecutorId &) const = default;
    };

    enum class CompletionSubscribeResult : uint8_t { ACCEPTED, BUSY, INVALID_CALLBACK, INVALID_EXECUTOR };

    using CompletionContinuation = std::function<void(const CompletionResult &)>;

    class ICompletion {
    public:
        virtual ~ICompletion() = default;
        // Thread-safe; publish once after started kernels exit and unstarted nodes are accounted for.
        // Terminal reads observe published kernel writes.
        virtual CompletionSnapshot Poll() const             = 0;
        virtual void               RequestCancel() noexcept = 0;

        // Thread-safe: ACCEPTED consumes/clears callback and reserves delivery capacity; other outcomes preserve it.
        // Completion/cancel races, including already-completed jobs, deliver once on the specified executor, never inline.
        // Pin executor/module through callback exit; queue saturation must not drop accepted callbacks.
        virtual CompletionSubscribeResult Subscribe(ContinuationExecutorId executor, CompletionContinuation &callback) = 0;
    };

    // Token destruction neither cancels nor proves drain; CPU data/arena leases must outlive continuations/readers.
    using CompletionToken = std::shared_ptr<ICompletion>;

} // namespace sky
