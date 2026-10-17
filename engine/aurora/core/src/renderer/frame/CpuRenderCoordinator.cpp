#include "frame/CpuRenderCoordinator.h"

#include <atomic>
#include <thread>
#include <type_traits>

namespace sky::aurora {

    static_assert(std::is_nothrow_move_assignable_v<CpuCoordinatorEvent>);
    static_assert(std::is_nothrow_move_constructible_v<CpuPacketCompleted>);

    struct CpuRenderCoordinator::State {
        State(const CpuCoordinatorInit &info, CpuPrepareBuilder builder, std::unique_ptr<CpuCoordinatorRuntime> runtime)
            : results(info.maxOutstandingResults), builder(std::move(builder)), prepareInfo(info.prepare), runtime(std::move(runtime))
        {
        }

        void Close()
        {
            auto expected = CpuCoordinatorState::RUNNING;
            phase.compare_exchange_strong(expected, CpuCoordinatorState::STOPPING);
            input.Close();
        }

        void Publish(CpuPacketCompleted result)
        {
            std::lock_guard lock(resultMutex);
            // Acceptance reserves a result slot until PollEvents consumes it; shutdown never waits for the host.
            SKY_ASSERT(resultCount < results.size());
            results[(resultHead + resultCount) % results.size()].emplace(std::move(result));
            ++resultCount;
        }

        std::optional<RendererError> Prepare(CpuSceneStore &store, CpuPacketCompleted &result)
        {
            auto frame = store.Freeze();
            if (!frame) {
                result.preparation.error = "CPU frame could not be frozen";
                return RendererError{RendererErrorCode::NOT_READY, result.preparation.error};
            }
            auto built = runtime ? runtime->Build(*frame) : (builder ? builder(*frame) : CpuPrepareBuildResult{CpuPrepareProgram{}});
            if (auto *error = std::get_if<RendererError>(&built)) {
                result.preparation.error = error->message.empty() ? "Prepare builder failed" : std::move(error->message);
                return RendererError{error->code, result.preparation.error};
            }
            auto program   = std::move(std::get<CpuPrepareProgram>(built));
            auto submitted = scheduler->Submit(*frame, std::move(program.systems), std::move(program.kernel), std::move(program.partitions),
                                               std::move(program.outputs));
            if (!submitted.Accepted()) {
                result.preparation.error =
                    submitted.error && !submitted.error->message.empty() ? std::move(submitted.error->message) : "Prepare submission failed";
                return RendererError{submitted.error ? submitted.error->code : RendererErrorCode::INTERNAL_ERROR, result.preparation.error};
            }
            {
                std::lock_guard lock(prepareMutex);
                activePrepare = submitted.group->GetCompletion();
            }
            frame.reset();
            bool                   resumed      = false;
            CompletionContinuation continuation = [&, group = submitted.group, completed = std::move(program.completed)](const CompletionResult &) {
                result.preparation = group->Wait();
                if (completed)
                    completed(result.preparation);
                resumed = true;
            };
            const auto subscribed = submitted.group->GetCompletion()->Subscribe(continuations->Id(), continuation);
            if (subscribed != CompletionSubscribeResult::ACCEPTED) {
                submitted.group->RequestCancel();
                result.preparation       = submitted.group->Wait();
                result.preparation.error = "Coordinator completion reservation failed";
            } else {
                while (!resumed)
                    continuations->Pump(true);
            }
            {
                std::lock_guard lock(prepareMutex);
                activePrepare.reset();
            }
            if (result.preparation.Succeeded() && result.preparation.products && !result.preparation.products->deferred.commands.empty()) {
                pendingDeferred = result.preparation.products;
            }
            if (!result.preparation.Succeeded()) {
                return RendererError{RendererErrorCode::INTERNAL_ERROR, "CPU prepare failed"};
            }
            return {};
        }

        void Run()
        {
            continuations = ContinuationExecutor::Create(1);
            std::optional<RendererError>   failure;
            std::unique_ptr<CpuSceneStore> store;
            auto                           created = ScenePrepareScheduler::Create(prepareInfo);
            scheduler                              = std::move(created.scheduler);
            if (!scheduler || !continuations) {
                failure = created.error
                              ? std::move(created.error)
                              : std::optional<RendererError>{{RendererErrorCode::INTERNAL_ERROR, "Could not create coordinator completion executor"}};
                Close();
            } else {
                store = std::make_unique<CpuSceneStore>();
                if (runtime)
                    failure = runtime->Initialize();
            }
            {
                std::lock_guard lock(startMutex);
                startError = failure;
                started    = true;
            }
            startReady.notify_one();
            if (failure)
                Close();
            for (;;) {
                if (runtime && store && !failure) {
                    failure = runtime->Pump(*store);
                    if (failure)
                        Close();
                }
                auto packet = runtime ? input.WaitPopFor(std::chrono::milliseconds(1)) : input.WaitPop();
                if (!packet) {
                    if (input.IsClosed())
                        break;
                    continue;
                }
                CpuPacketCompleted result;
                result.application.sequence = packet->sequence;
                if (failure) {
                    result.application.error = RendererError{RendererErrorCode::NOT_READY, {}};
                } else {
                    if (runtime)
                        result.application.error = runtime->Validate(*packet);
                    if (!result.application.error)
                        result.application = store->Apply(*packet, pendingDeferred ? &pendingDeferred->deferred : nullptr, runtime != nullptr);
                    if (result.application.Succeeded()) {
                        pendingDeferred.reset();
                        if (runtime)
                            failure = runtime->Applied(*store, *packet, result.application);
                        if (!failure) {
                            result.preparationAttempted = true;
                            failure                     = Prepare(*store, result);
                            if (failure && runtime && runtime->RecoverPrepareFailure(result))
                                failure.reset();
                        }
                    } else if (store->IsFailed()) {
                        failure = RendererError{result.application.error->code, {}};
                    }
                    if (failure) {
                        Close();
                    }
                    // Prepare returning (including failure) does not prove that captured contexts/read views have drained.
                    store->WaitUntilUnfrozen();
                    if (runtime && result.application.Succeeded()) {
                        auto execution = runtime->Prepared(*store, result);
                        if (execution && !failure)
                            failure = std::move(execution);
                        result.executionError = failure;
                    }
                    if (failure)
                        Close();
                }
                Publish(std::move(result));
            }
            if (runtime && store) {
                auto shutdown = runtime->Shutdown(*store);
                if (shutdown && !failure)
                    failure = std::move(shutdown);
            }
            runtime.reset();
            store.reset();
            scheduler.reset();
            if (continuations) {
                continuations->Close();
                SKY_ASSERT(continuations->IsDrained());
                continuations.reset();
            }
            {
                std::lock_guard lock(resultMutex);
                const auto      discarded = pendingDeferred ? pendingDeferred->deferred.commands.size() : 0;
                pendingDeferred.reset();
                stopped.emplace(CpuCoordinatorStopped{std::move(failure), discarded});
                phase.store(CpuCoordinatorState::STOPPED);
            }
        }

        FramePacketQueue                               input;
        std::mutex                                     resultMutex;
        std::vector<std::optional<CpuPacketCompleted>> results;
        size_t                                         resultHead           = 0;
        size_t                                         resultCount          = 0;
        size_t                                         outstanding          = 0;
        uint64_t                                       lastAcceptedSequence = 0;
        std::optional<CpuCoordinatorStopped>           stopped;
        CpuPrepareBuilder                              builder;
        std::unique_ptr<CpuCoordinatorRuntime>         runtime;
        std::mutex                                     startMutex;
        std::condition_variable                        startReady;
        bool                                           started = false;
        std::optional<RendererError>                   startError;
        ScenePrepareInit                               prepareInfo;
        std::unique_ptr<ScenePrepareScheduler>         scheduler;
        std::shared_ptr<ContinuationExecutor>          continuations;
        std::mutex                                     prepareMutex;
        CompletionToken                                activePrepare;
        std::shared_ptr<const PrepareProducts>         pendingDeferred;
        std::atomic<CpuCoordinatorState>               phase{CpuCoordinatorState::RUNNING};
        std::thread                                    worker;
    };

    CpuRenderCoordinator::CpuRenderCoordinator() = default;
    std::optional<RendererError>
    CpuRenderCoordinator::Init(const CpuCoordinatorInit &info, CpuPrepareBuilder builder, std::unique_ptr<CpuCoordinatorRuntime> runtime)
    {
        if (state)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Coordinator is already initialized"};
        if (!info.maxPendingPackets || !info.maxOutstandingResults || !info.prepare.workers)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Coordinator capacities and workers must be nonzero"};
        if (builder && runtime)
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Runtime owns its prepare builder"};
        auto created = std::make_unique<State>(info, std::move(builder), std::move(runtime));
        if (!created->input.Init(info.maxPendingPackets))
            return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Invalid packet queue"};
        created->worker = std::thread([raw = created.get()] { raw->Run(); });
        {
            std::unique_lock lock(created->startMutex);
            created->startReady.wait(lock, [&] { return created->started; });
        }
        if (created->startError) {
            created->worker.join();
            return created->startError;
        }
        state = std::move(created);
        return {};
    }

    CpuRenderCoordinator::~CpuRenderCoordinator()
    {
        RequestShutdown();
        Join();
    }

    PacketSubmitResult CpuRenderCoordinator::TrySubmit(std::unique_ptr<FramePacket> &packet)
    {
        if (!state)
            return {PacketSubmitStatus::REJECTED, {RendererErrorCode::NOT_READY, "Coordinator is not initialized"}};
        std::lock_guard lock(state->resultMutex);
        if (state->phase.load() != CpuCoordinatorState::RUNNING) {
            return {PacketSubmitStatus::REJECTED, {RendererErrorCode::NOT_READY, "CPU coordinator is stopping"}};
        }
        if (!packet || packet->sequence == 0 || packet->sequence <= state->lastAcceptedSequence) {
            return {PacketSubmitStatus::REJECTED, {RendererErrorCode::INVALID_PACKET, "Invalid packet sequence"}};
        }
        if (state->outstanding == state->results.size()) {
            return {PacketSubmitStatus::BUSY};
        }
        const auto sequence = packet->sequence;
        auto       result   = state->input.TrySubmit(packet);
        if (result.status == PacketSubmitStatus::ACCEPTED) {
            ++state->outstanding;
            state->lastAcceptedSequence = sequence;
        }
        return result;
    }

    size_t CpuRenderCoordinator::PollEvents(std::span<CpuCoordinatorEvent> destination)
    {
        if (!state)
            return 0;
        std::lock_guard lock(state->resultMutex);
        size_t          count = 0;
        while (count < destination.size() && state->resultCount != 0) {
            destination[count++] = std::move(*state->results[state->resultHead]);
            state->results[state->resultHead].reset();
            state->resultHead = (state->resultHead + 1) % state->results.size();
            --state->resultCount;
            --state->outstanding;
        }
        if (count < destination.size() && state->resultCount == 0 && state->stopped) {
            destination[count++] = std::move(*state->stopped);
            state->stopped.reset();
        }
        return count;
    }

    CpuCoordinatorState CpuRenderCoordinator::GetState() const
    {
        return state ? state->phase.load() : CpuCoordinatorState::STOPPED;
    }
    void CpuRenderCoordinator::RequestShutdown()
    {
        if (state)
            state->Close();
    }
    bool CpuRenderCoordinator::RequestCancelPrepare()
    {
        if (!state)
            return false;
        std::lock_guard lock(state->prepareMutex);
        if (!state->activePrepare || state->activePrepare->Poll().result.state != CompletionState::PENDING)
            return false;
        state->activePrepare->RequestCancel();
        return true;
    }
    void CpuRenderCoordinator::Join()
    {
        if (state && state->worker.joinable()) {
            state->worker.join();
        }
    }

} // namespace sky::aurora
