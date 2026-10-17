#include <core/async/Completion.h>

#include <core/environment/Singleton.h>
#include <core/platform/Platform.h>

#include <atomic>
#include <condition_variable>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace sky {

    class CompletionExecutorRegistry : public Singleton<CompletionExecutorRegistry> {
        friend class Singleton<CompletionExecutorRegistry>;

    public:
        ContinuationExecutorId Add(const std::shared_ptr<ContinuationExecutor> &executor)
        {
            std::lock_guard lock(mutex);
            if (next == std::numeric_limits<uint64_t>::max())
                return {};
            const ContinuationExecutorId id{++next, 1};
            executors.emplace(id.id, executor);
            return id;
        }
        std::shared_ptr<ContinuationExecutor> Find(ContinuationExecutorId id)
        {
            std::lock_guard lock(mutex);
            const auto      found = executors.find(id.id);
            return id.generation == 1 && found != executors.end() ? found->second.lock() : nullptr;
        }
        void Remove(ContinuationExecutorId id)
        {
            std::lock_guard lock(mutex);
            executors.erase(id.id);
        }

    private:
        std::mutex                                              mutex;
        uint64_t                                                next = 0;
        std::map<uint64_t, std::weak_ptr<ContinuationExecutor>> executors;
    };

    struct ContinuationExecutor::Delivery {
        std::shared_ptr<ContinuationExecutor>   executor;
        CompletionContinuation                  callback;
        std::shared_ptr<const CompletionResult> result;
    };

    struct ContinuationExecutor::State {
        State(size_t capacity, std::shared_ptr<void> pin) : keepAlive(std::move(pin)), queue(capacity), owner(std::this_thread::get_id())
        {
        }
        std::shared_ptr<void>                  keepAlive;
        std::vector<std::shared_ptr<Delivery>> queue;
        const std::thread::id                  owner;
        ContinuationExecutorId                 id;
        mutable std::mutex                     mutex;
        std::condition_variable                changed;
        size_t                                 head     = 0;
        size_t                                 count    = 0;
        size_t                                 reserved = 0;
        bool                                   closed   = false;
        bool                                   pumping  = false;
    };

    ContinuationExecutor::ContinuationExecutor(size_t capacity, std::shared_ptr<void> keepAlive)
        : state(std::make_unique<State>(capacity, std::move(keepAlive)))
    {
    }

    std::shared_ptr<ContinuationExecutor> ContinuationExecutor::Create(size_t capacity, std::shared_ptr<void> keepAlive)
    {
        if (capacity == 0)
            return {};
        auto executor       = std::shared_ptr<ContinuationExecutor>(new ContinuationExecutor(capacity, std::move(keepAlive)));
        executor->state->id = CompletionExecutorRegistry::Get()->Add(executor);
        return executor->state->id.id ? executor : nullptr;
    }

    ContinuationExecutor::~ContinuationExecutor()
    {
        SKY_ASSERT(IsDrained());
        CompletionExecutorRegistry::Get()->Remove(state->id);
    }

    ContinuationExecutorId ContinuationExecutor::Id() const
    {
        return state->id;
    }

    CompletionSubscribeResult ContinuationExecutor::Reserve(CompletionContinuation &callback, std::shared_ptr<Delivery> &delivery)
    {
        std::lock_guard lock(state->mutex);
        if (state->closed)
            return CompletionSubscribeResult::INVALID_EXECUTOR;
        if (state->reserved == state->queue.size())
            return CompletionSubscribeResult::BUSY;
        delivery           = std::make_shared<Delivery>();
        delivery->executor = shared_from_this();
        delivery->callback = std::move(callback);
        callback           = {};
        ++state->reserved;
        return CompletionSubscribeResult::ACCEPTED;
    }

    void ContinuationExecutor::Enqueue(std::shared_ptr<Delivery> delivery, std::shared_ptr<const CompletionResult> result)
    {
        std::lock_guard lock(state->mutex);
        SKY_ASSERT(state->count < state->queue.size());
        delivery->result                                                 = std::move(result);
        state->queue[(state->head + state->count) % state->queue.size()] = std::move(delivery);
        ++state->count;
        state->changed.notify_one();
    }

    ContinuationPumpResult ContinuationExecutor::Pump(bool wait)
    {
        if (std::this_thread::get_id() != state->owner)
            return ContinuationPumpResult::INVALID_THREAD;
        // A callback may release the owner's handle; accepted delivery still pins this executor through exit.
        const auto                pin = shared_from_this();
        std::shared_ptr<Delivery> delivery;
        {
            std::unique_lock lock(state->mutex);
            if (state->pumping)
                return ContinuationPumpResult::REENTRANT;
            if (wait)
                state->changed.wait(lock, [&] { return state->count || (state->closed && state->reserved == 0); });
            if (state->count == 0)
                return ContinuationPumpResult::IDLE;
            delivery    = std::move(state->queue[state->head]);
            state->head = (state->head + 1) % state->queue.size();
            --state->count;
            state->pumping = true;
        }
        delivery->callback(*delivery->result);
        delivery.reset();
        {
            std::lock_guard lock(state->mutex);
            state->pumping = false;
            --state->reserved;
            state->changed.notify_all();
        }
        return ContinuationPumpResult::EXECUTED;
    }

    void ContinuationExecutor::Close()
    {
        std::lock_guard lock(state->mutex);
        state->closed = true;
        state->changed.notify_all();
    }

    bool ContinuationExecutor::IsDrained() const
    {
        std::lock_guard lock(state->mutex);
        return state->reserved == 0;
    }

    struct CompletionSource::State {
        mutable std::mutex                                           mutex;
        mutable std::condition_variable                              changed;
        std::atomic_bool                                             cancelRequested = false;
        std::shared_ptr<const CompletionResult>                      result;
        std::vector<std::shared_ptr<ContinuationExecutor::Delivery>> subscriptions;
    };

    CompletionSource::CompletionSource() : state(std::make_unique<State>())
    {
    }
    CompletionSource::~CompletionSource()
    {
        SKY_ASSERT(state->subscriptions.empty());
    }

    CompletionSnapshot CompletionSource::Poll() const
    {
        std::lock_guard lock(state->mutex);
        return {state->result ? *state->result : CompletionResult{}, IsCancelRequested()};
    }

    void CompletionSource::RequestCancel() noexcept
    {
        std::lock_guard lock(state->mutex);
        if (!state->result)
            state->cancelRequested.store(true, std::memory_order_release);
    }

    bool CompletionSource::IsCancelRequested() const noexcept
    {
        return state->cancelRequested.load(std::memory_order_acquire);
    }

    CompletionSubscribeResult CompletionSource::Subscribe(ContinuationExecutorId id, CompletionContinuation &callback)
    {
        if (!callback)
            return CompletionSubscribeResult::INVALID_CALLBACK;
        const auto executor = CompletionExecutorRegistry::Get()->Find(id);
        if (!executor)
            return CompletionSubscribeResult::INVALID_EXECUTOR;
        std::lock_guard                                 lock(state->mutex);
        std::shared_ptr<ContinuationExecutor::Delivery> delivery;
        const auto                                      accepted = executor->Reserve(callback, delivery);
        if (accepted != CompletionSubscribeResult::ACCEPTED)
            return accepted;
        if (state->result)
            executor->Enqueue(std::move(delivery), state->result);
        else
            state->subscriptions.push_back(std::move(delivery));
        return accepted;
    }

    bool CompletionSource::Publish(CompletionResult result)
    {
        if (result.state == CompletionState::PENDING)
            return false;
        std::lock_guard lock(state->mutex);
        if (state->result)
            return false;
        state->result = std::make_shared<const CompletionResult>(std::move(result));
        for (auto &delivery : state->subscriptions) {
            auto executor = delivery->executor;
            executor->Enqueue(std::move(delivery), state->result);
        }
        state->subscriptions.clear();
        state->changed.notify_all();
        return true;
    }

    CompletionResult CompletionSource::Wait() const
    {
        std::unique_lock lock(state->mutex);
        state->changed.wait(lock, [&] { return state->result != nullptr; });
        return *state->result;
    }

} // namespace sky
