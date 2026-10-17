#pragma once

#include <cstddef>
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

    enum class ContinuationPumpResult : uint8_t { EXECUTED, IDLE, INVALID_THREAD, REENTRANT };

    // Created and pumped on the destination thread; Close rejects new reservations but preserves accepted deliveries.
    // The owner must pump until drained before unloading code. Callback captures must pin their own module/resources.
    class ContinuationExecutor : public std::enable_shared_from_this<ContinuationExecutor> {
    public:
        static std::shared_ptr<ContinuationExecutor> Create(size_t capacity, std::shared_ptr<void> keepAlive = {});
        ~ContinuationExecutor();
        ContinuationExecutorId Id() const;
        ContinuationPumpResult Pump(bool wait = false);
        void                   Close();
        bool                   IsDrained() const;

    private:
        friend class CompletionSource;
        struct Delivery;
        struct State;
        explicit ContinuationExecutor(size_t capacity, std::shared_ptr<void> keepAlive);
        CompletionSubscribeResult Reserve(CompletionContinuation &callback, std::shared_ptr<Delivery> &delivery);
        void                      Enqueue(std::shared_ptr<Delivery> delivery, std::shared_ptr<const CompletionResult> result);
        std::unique_ptr<State>    state;
    };

    // Producer owns terminal publication after its work drains; dropping a token does not publish or cancel.
    class CompletionSource final : public ICompletion {
    public:
        CompletionSource();
        ~CompletionSource() override;
        CompletionSnapshot        Poll() const override;
        void                      RequestCancel() noexcept override;
        bool                      IsCancelRequested() const noexcept;
        CompletionSubscribeResult Subscribe(ContinuationExecutorId executor, CompletionContinuation &callback) override;
        bool                      Publish(CompletionResult result);
        // Owner/join path only; never wait inside a worker of the producing pool.
        CompletionResult Wait() const;

    private:
        struct State;
        std::unique_ptr<State> state;
    };

} // namespace sky
