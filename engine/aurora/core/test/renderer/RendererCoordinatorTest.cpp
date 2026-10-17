#include "frame/CpuRenderCoordinator.h"
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    constexpr SceneId         scene{0, 1};
    constexpr RenderEntityRef entity{scene, {0, 1}};
    constexpr ViewportId      output{0, 1};
    constexpr ViewId          view{0, 1};

    std::unique_ptr<FramePacket> Packet(uint64_t sequence, std::vector<RenderCommand> commands = {})
    {
        auto packet      = std::make_unique<FramePacket>();
        packet->sequence = sequence;
        packet->commands = std::move(commands);
        return packet;
    }

    std::unique_ptr<FramePacket> Initial()
    {
        ViewDesc camera;
        camera.scene  = scene;
        camera.output = output;
        RenderEntityDesc object;
        object.world.v[12] = 7;
        return Packet(1, {CreateScene{scene, {}}, CreateRenderEntity{entity, object}, CreateViewport{output, {}}, CreateView{view, camera}});
    }

    SystemDesc Reader(SystemId id = 1)
    {
        SystemDesc desc{id, "ReadTransform"};
        desc.accesses = {ComponentAccess<RenderTransform>("scene", SceneAccessOwner(scene), AccessMode::READ)};
        return desc;
    }

    struct ScopeExit {
        std::function<void()> fn;
        ~ScopeExit()
        {
            fn();
        }
    };

    struct BorrowSlot {
        void Capture(FrozenReadView<RenderTransform> view)
        {
            std::lock_guard lock(mutex);
            if (enabled) {
                value.emplace(std::move(view));
            }
        }
        void Release()
        {
            std::lock_guard lock(mutex);
            enabled = false;
            value.reset();
        }
        std::mutex                                     mutex;
        bool                                           enabled = true;
        std::optional<FrozenReadView<RenderTransform>> value;
    };

    struct Gate {
        Gate() : future(promise.get_future().share())
        {
        }
        void Release()
        {
            if (!released.exchange(true)) {
                promise.set_value();
            }
        }
        std::promise<void>       promise;
        std::shared_future<void> future;
        std::atomic_bool         released = false;
    };

    template <typename Fn>
    bool Until(Fn &&fn)
    {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        do {
            if (fn()) {
                return true;
            }
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    std::vector<CpuCoordinatorEvent> Drain(CpuRenderCoordinator &coordinator)
    {
        std::vector<CpuCoordinatorEvent>   events;
        std::array<CpuCoordinatorEvent, 2> batch;
        for (size_t count; (count = coordinator.PollEvents(batch)) != 0;) {
            for (size_t i = 0; i < count; ++i) {
                events.push_back(std::move(batch[i]));
            }
        }
        return events;
    }
} // namespace

TEST(FrozenCpuFrame, ReadViewPinsStorageAndBlocksMutationWithoutConsumingSequence)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
    auto frame = store.Freeze();
    ASSERT_TRUE(frame);
    EXPECT_FALSE(store.Freeze());
    EXPECT_EQ(frame->Sequence(), 1u);
    EXPECT_EQ(frame->Scenes().size(), 1u);
    EXPECT_EQ(frame->Views()[0].view.id, view);
    auto read = frame->Bind(Reader()).context->Read<RenderTransform>(scene);
    frame.reset();
    EXPECT_TRUE(store.IsFrozen());
    auto next    = Packet(2, {DestroyRenderEntity{entity}});
    auto blocked = store.Apply(*next);
    ASSERT_FALSE(blocked.Succeeded());
    EXPECT_EQ(blocked.error->code, RendererErrorCode::NOT_READY);
    EXPECT_FALSE(store.AcknowledgeRetirement(entity));
    EXPECT_EQ(read.Size(), 1u);
    read.ForEach([](RenderEntityId id, const auto &transform) {
        static_assert(std::is_const_v<std::remove_reference_t<decltype(transform)>>);
        EXPECT_EQ(id, entity.entity);
        EXPECT_EQ(transform.world.v[12], 7);
    });
    read = {};
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_TRUE(store.Apply(*next).Succeeded());
    EXPECT_EQ(store.FindScene(scene)->FindEntity(entity.entity), nullptr);
}

TEST(FrozenCpuFrame, BorrowOutlivesStoreOwnerAndReleasesOnWorker)
{
    FrozenReadView<RenderTransform> read;
    {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
        read = store.Freeze()->Bind(Reader()).context->Read<RenderTransform>(scene);
    }
    std::thread worker([read = std::move(read)] {
        EXPECT_EQ(read.Size(), 1u);
        read.ForEach([](RenderEntityId, const RenderTransform &transform) { EXPECT_EQ(transform.world.v[12], 7); });
    });
    worker.join();
}

TEST(FrozenCpuFrame, RetirementWaitsForFreezeAndRejectedPacketsDoNotRelabelState)
{
    CpuSceneStore store;
    EXPECT_FALSE(store.Freeze());
    ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
    ASSERT_TRUE(store.Apply(*Packet(2, {DestroyRenderEntity{entity}})).Succeeded());
    EXPECT_FALSE(store.Apply(*Packet(3, {UpdateView{{}, {}, 0}})).Succeeded());
    auto frame = store.Freeze();
    ASSERT_TRUE(frame);
    EXPECT_EQ(frame->Sequence(), 2u);
    EXPECT_FALSE(store.AcknowledgeRetirement(entity));
    frame.reset();
    EXPECT_TRUE(store.AcknowledgeRetirement(entity));
}

TEST(FrozenCpuFrame, DeclarationsRestrictTypesOwnersAndWrites)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
    auto frame   = store.Freeze();
    auto context = frame->Bind(Reader());
    ASSERT_TRUE(context.context);
    EXPECT_FALSE(context.context->Read<RenderVisibility>(scene));
    EXPECT_FALSE(context.context->Read<RenderTransform>((SceneId{0, 2})));
    auto write             = Reader();
    write.accesses[0].mode = AccessMode::WRITE;
    EXPECT_TRUE(frame->Bind(write).error);
    auto stale                             = Reader();
    stale.accesses[0].key.owner.generation = 2;
    EXPECT_TRUE(frame->Bind(stale).error);
    auto wrongTag            = Reader();
    wrongTag.accesses[0].tag = "wrong tag";
    EXPECT_TRUE(frame->Bind(wrongTag).error);
    auto both = Reader();
    both.accesses.push_back(ComponentAccess<RenderVisibility>("scene", SceneAccessOwner(scene), AccessMode::READ));
    auto joined = frame->Bind(both).context->Read<RenderTransform, RenderVisibility>(scene);
    EXPECT_EQ(joined.Size(), 1u);
}

TEST(CpuRenderCoordinator, BorrowDrainPrecedesNextPacketAndShutdownDoesNotWaitForEventConsumption)
{
    auto                 borrowed = std::make_shared<BorrowSlot>();
    std::promise<void>   captured;
    auto                 ready  = captured.get_future();
    std::atomic_uint32_t runs   = 0;
    const auto           caller = std::this_thread::get_id();
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 2}, [&](const FrozenCpuFrame &) {
        EXPECT_NE(std::this_thread::get_id(), caller);
        return CpuPrepareProgram{{Reader()}, [&](const FrozenPrepareContext &context) {
                                     ++runs;
                                     auto read = context.Read<RenderTransform>(scene);
                                     if (context.Sequence() == 1) {
                                         borrowed->Capture(std::move(read));
                                         captured.set_value();
                                     } else {
                                         EXPECT_EQ(read.Size(), 0u);
                                     }
                                 }};
    }));
    ScopeExit release{[&] { borrowed->Release(); }};
    auto      first = Initial();
    ASSERT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
    auto second = Packet(2, {DestroyRenderEntity{entity}});
    ASSERT_EQ(coordinator.TrySubmit(second).status, PacketSubmitStatus::ACCEPTED);
    auto  third    = Packet(3);
    auto *original = third.get();
    EXPECT_EQ(coordinator.TrySubmit(third).status, PacketSubmitStatus::BUSY);
    EXPECT_EQ(third.get(), original);
    std::array<CpuCoordinatorEvent, 1> event;
    EXPECT_EQ(coordinator.PollEvents(event), 0u);
    EXPECT_EQ(runs.load(), 1u);
    coordinator.RequestShutdown();
    coordinator.RequestShutdown();
    EXPECT_EQ(coordinator.GetState(), CpuCoordinatorState::STOPPING);
    EXPECT_EQ(coordinator.TrySubmit(third).status, PacketSubmitStatus::REJECTED);
    borrowed->Release();
    coordinator.Join();
    EXPECT_EQ(coordinator.GetState(), CpuCoordinatorState::STOPPED);
    EXPECT_EQ(coordinator.PollEvents({}), 0u);
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 3u);
    for (size_t i = 0; i < 2; ++i) {
        const auto &packet = std::get<CpuPacketCompleted>(events[i]);
        EXPECT_EQ(packet.application.sequence, i + 1);
        EXPECT_TRUE(packet.Succeeded());
    }
    EXPECT_EQ(std::get<CpuPacketCompleted>(events[1]).application.retirements.size(), 1u);
    EXPECT_FALSE(std::get<CpuCoordinatorStopped>(events[2]).error);
    EXPECT_TRUE(Drain(coordinator).empty());
}

TEST(CpuRenderCoordinator, PollingReturnsResultCapacityAndZeroCapacityConsumesNothing)
{
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 1}));
    auto first = Packet(1);
    ASSERT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    auto second = Packet(2);
    EXPECT_EQ(coordinator.TrySubmit(second).status, PacketSubmitStatus::BUSY);
    std::unique_ptr<FramePacket> missing;
    EXPECT_EQ(coordinator.TrySubmit(missing).status, PacketSubmitStatus::REJECTED);
    auto duplicate = Packet(1);
    EXPECT_EQ(coordinator.TrySubmit(duplicate).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(duplicate);
    EXPECT_EQ(coordinator.PollEvents({}), 0u);
    EXPECT_EQ(coordinator.TrySubmit(second).status, PacketSubmitStatus::BUSY);
    std::array<CpuCoordinatorEvent, 1> event;
    ASSERT_TRUE(Until([&] { return coordinator.PollEvents(event) == 1; }));
    EXPECT_TRUE(std::get<CpuPacketCompleted>(event[0]).Succeeded());
    ASSERT_EQ(coordinator.TrySubmit(second).status, PacketSubmitStatus::ACCEPTED);
    coordinator.RequestShutdown();
    coordinator.Join();
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(std::get<CpuPacketCompleted>(events[0]).application.sequence, 2u);
    EXPECT_TRUE(std::holds_alternative<CpuCoordinatorStopped>(events[1]));
}

TEST(CpuRenderCoordinator, MalformedPacketReportsFailureAndLaterPacketStillApplies)
{
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 2}));
    auto invalid = Packet(1, {CreateScene{scene, {}}, UpdateView{{}, {}, 0}});
    auto valid   = Packet(2, {CreateScene{scene, {}}});
    ASSERT_EQ(coordinator.TrySubmit(invalid).status, PacketSubmitStatus::ACCEPTED);
    ASSERT_EQ(coordinator.TrySubmit(valid).status, PacketSubmitStatus::ACCEPTED);
    coordinator.RequestShutdown();
    coordinator.Join();
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 3u);
    const auto &first = std::get<CpuPacketCompleted>(events[0]);
    EXPECT_FALSE(first.Succeeded());
    EXPECT_FALSE(first.preparationAttempted);
    ASSERT_TRUE(first.application.error);
    EXPECT_EQ(first.application.error->code, RendererErrorCode::INVALID_PACKET);
    const auto &second = std::get<CpuPacketCompleted>(events[1]);
    EXPECT_TRUE(second.Succeeded());
    EXPECT_EQ(second.application.changedScenes, (std::vector<SceneId>{scene}));
    EXPECT_FALSE(std::get<CpuCoordinatorStopped>(events[2]).error);
}

TEST(CpuRenderCoordinator, FailedKernelDrainsBorrowersAndAccountsForQueuedPackets)
{
    auto                 gate     = std::make_shared<Gate>();
    auto                 borrowed = std::make_shared<BorrowSlot>();
    std::promise<void>   entered;
    auto                 ready       = entered.get_future();
    std::atomic_uint32_t consumers   = 0;
    std::atomic_uint32_t independent = 0;
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 3}, [&](const FrozenCpuFrame &) {
        auto       source = Reader();
        SystemDesc consumer{2, "Consumer"};
        consumer.after = {1};
        SystemDesc other{3, "Independent"};
        return CpuPrepareProgram{{source, consumer, other}, [&](const FrozenPrepareContext &context) {
                                     if (context.Id() == 1) {
                                         borrowed->Capture(context.Read<RenderTransform>(scene));
                                         entered.set_value();
                                         gate->future.wait();
                                         context.Fail("injected prepare failure");
                                     }
                                     if (context.Id() == 2) {
                                         ++consumers;
                                     }
                                     if (context.Id() == 3) {
                                         ++independent;
                                     }
                                 }};
    }));
    ScopeExit release{[&] {
        gate->Release();
        borrowed->Release();
    }};
    auto      first = Initial();
    ASSERT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
    auto queued = Packet(2, {DestroyScene{scene}});
    ASSERT_EQ(coordinator.TrySubmit(queued).status, PacketSubmitStatus::ACCEPTED);
    gate->Release();
    EXPECT_TRUE(Until([&] { return coordinator.GetState() == CpuCoordinatorState::STOPPING; }));
    std::array<CpuCoordinatorEvent, 1> event;
    EXPECT_EQ(coordinator.PollEvents(event), 0u);
    borrowed->Release();
    coordinator.Join();
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 3u);
    const auto &failed = std::get<CpuPacketCompleted>(events[0]);
    EXPECT_TRUE(failed.application.Succeeded());
    EXPECT_FALSE(failed.Succeeded());
    ASSERT_EQ(failed.preparation.nodes.size(), 3u);
    EXPECT_EQ(failed.preparation.nodes[0].diagnostic, "injected prepare failure");
    EXPECT_EQ(failed.preparation.nodes[1].state, PrepareNodeState::SKIPPED);
    EXPECT_EQ(consumers.load(), 0u);
    EXPECT_EQ(independent.load(), 1u);
    EXPECT_FALSE(std::get<CpuPacketCompleted>(events[1]).Succeeded());
    EXPECT_EQ(std::get<CpuPacketCompleted>(events[1]).application.sequence, 2u);
    EXPECT_TRUE(std::get<CpuCoordinatorStopped>(events[2]).error);
}

TEST(CpuRenderCoordinator, PrebindingAllContextsPreventsPartialKernelExecution)
{
    std::atomic_uint32_t calls = 0;
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({1, 1}, [&](const FrozenCpuFrame &) {
        auto valid               = Reader();
        auto invalid             = Reader(2);
        invalid.accesses[0].mode = AccessMode::WRITE;
        return CpuPrepareProgram{{valid, invalid}, [&](const FrozenPrepareContext &) { ++calls; }};
    }));
    auto packet = Initial();
    ASSERT_EQ(coordinator.TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
    EXPECT_TRUE(Until([&] { return coordinator.GetState() == CpuCoordinatorState::STOPPED; }));
    coordinator.RequestShutdown();
    coordinator.Join();
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_FALSE(std::get<CpuPacketCompleted>(events[0]).Succeeded());
    EXPECT_FALSE(std::get<CpuPacketCompleted>(events[0]).preparation.error.empty());
    EXPECT_EQ(calls.load(), 0u);
    EXPECT_TRUE(std::get<CpuCoordinatorStopped>(events[1]).error);
}

TEST(CpuRenderCoordinator, IdleShutdownAndInvalidCapacity)
{
    CpuRenderCoordinator coordinator;
    auto                 packet = Packet(1);
    EXPECT_EQ(coordinator.TrySubmit(packet).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(packet);
    EXPECT_TRUE(Drain(coordinator).empty());
    EXPECT_EQ(coordinator.GetState(), CpuCoordinatorState::STOPPED);
    coordinator.RequestShutdown();
    coordinator.Join();
    EXPECT_TRUE(coordinator.Init({0, 1}));
    EXPECT_TRUE(coordinator.Init({1, 0}));
    EXPECT_TRUE(coordinator.Init({1, 1, {0}}));
    ASSERT_FALSE(coordinator.Init({1, 1}));
    EXPECT_TRUE(coordinator.Init({1, 1}));
    coordinator.RequestShutdown();
    coordinator.Join();
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_FALSE(std::get<CpuCoordinatorStopped>(events[0]).error);
}

TEST(CpuRenderCoordinator, BuilderErrorReportsDiagnosticAndCompletesAcceptedPackets)
{
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 2}, [](const FrozenCpuFrame &) -> CpuPrepareBuildResult {
        return RendererError{RendererErrorCode::INVALID_ARGUMENT, "invalid prepare declaration"};
    }));
    auto first = Initial();
    ASSERT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    coordinator.Join();
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 2u);
    const auto &completed = std::get<CpuPacketCompleted>(events[0]);
    EXPECT_TRUE(completed.application.Succeeded());
    EXPECT_TRUE(completed.preparationAttempted);
    EXPECT_FALSE(completed.Succeeded());
    EXPECT_EQ(completed.preparation.error, "invalid prepare declaration");
    const auto &stopped = std::get<CpuCoordinatorStopped>(events[1]);
    ASSERT_TRUE(stopped.error);
    EXPECT_EQ(stopped.error->code, RendererErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(stopped.error->message, completed.preparation.error);
}

TEST(CpuRenderCoordinator, ConcurrentReceiptConsumptionPreservesOrderAcrossRingWrap)
{
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 3}));
    std::vector<uint64_t> sequences;
    auto                  packet = Packet(1);
    uint64_t              next   = 1;
    ASSERT_TRUE(Until([&] {
        if (coordinator.TrySubmit(packet).status == PacketSubmitStatus::ACCEPTED) {
            packet = Packet(++next);
        }
        for (const auto &event : Drain(coordinator)) {
            sequences.push_back(std::get<CpuPacketCompleted>(event).application.sequence);
        }
        return next == 201;
    }));
    coordinator.RequestShutdown();
    coordinator.Join();
    size_t stopped = 0;
    for (const auto &event : Drain(coordinator)) {
        if (const auto *result = std::get_if<CpuPacketCompleted>(&event)) {
            EXPECT_TRUE(result->Succeeded());
            sequences.push_back(result->application.sequence);
        } else {
            ++stopped;
        }
    }
    ASSERT_EQ(sequences.size(), 200u);
    EXPECT_EQ(stopped, 1u);
    for (size_t i = 0; i < sequences.size(); ++i) {
        EXPECT_EQ(sequences[i], i + 1);
    }
}

TEST(CpuRenderCoordinator, ContinuationResumesOnCoordinatorAndHoldsShutdownUntilExit)
{
    const auto           owner = std::this_thread::get_id();
    std::thread::id      buildThread;
    std::thread::id      kernelThread;
    auto                 gate = std::make_shared<Gate>();
    std::promise<void>   entered;
    auto                 ready         = entered.get_future();
    std::atomic_int      continuations = 0;
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({1, 1}, [&](const FrozenCpuFrame &) {
        buildThread = std::this_thread::get_id();
        CpuPrepareProgram program{{Reader()}, [&](const FrozenPrepareContext &) { kernelThread = std::this_thread::get_id(); }};
        program.completed = [&](const PrepareResult &result) {
            EXPECT_EQ(std::this_thread::get_id(), buildThread);
            EXPECT_NE(std::this_thread::get_id(), kernelThread);
            EXPECT_NE(std::this_thread::get_id(), owner);
            EXPECT_TRUE(result.Succeeded());
            ++continuations;
            entered.set_value();
            EXPECT_EQ(gate->future.wait_for(5s), std::future_status::ready);
        };
        return program;
    }));
    ScopeExit release{[&] { gate->Release(); }};
    auto      packet = Initial();
    ASSERT_EQ(coordinator.TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
    ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
    coordinator.RequestShutdown();
    EXPECT_EQ(coordinator.GetState(), CpuCoordinatorState::STOPPING);
    EXPECT_TRUE(Drain(coordinator).empty());
    gate->Release();
    coordinator.Join();
    EXPECT_EQ(continuations, 1);
    const auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_TRUE(std::get<CpuPacketCompleted>(events[0]).Succeeded());
}

TEST(CpuRenderCoordinator, CancellationDrainsKernelsThenConsumersAndRequiresRebuild)
{
    for (const bool serial : {false, true}) {
        SCOPED_TRACE(serial);
        auto                 gate     = std::make_shared<Gate>();
        auto                 borrowed = std::make_shared<BorrowSlot>();
        std::promise<void>   entered;
        auto                 ready         = entered.get_future();
        std::atomic_int      consumers     = 0;
        std::atomic_int      continuations = 0;
        CpuRenderCoordinator coordinator;
        CpuCoordinatorInit   info{2, 2};
        info.prepare.workers = 1;
        if (serial)
            info.prepare.maxParallelNodes = 0;
        ASSERT_FALSE(coordinator.Init(info, [&](const FrozenCpuFrame &) {
            auto dependent  = Reader(2);
            dependent.after = {1};
            CpuPrepareProgram program{{Reader(), dependent}, [&](const FrozenPrepareContext &context) {
                                          if (context.Id() == 1) {
                                              borrowed->Capture(context.Read<RenderTransform>(scene));
                                              entered.set_value();
                                              EXPECT_EQ(gate->future.wait_for(5s), std::future_status::ready);
                                          } else {
                                              ++consumers;
                                          }
                                      }};
            program.completed = [&](const PrepareResult &result) {
                EXPECT_TRUE(result.cancelled);
                EXPECT_FALSE(result.products);
                ++continuations;
            };
            return program;
        }));
        ScopeExit release{[&] {
            gate->Release();
            borrowed->Release();
        }};
        auto      first = Initial();
        ASSERT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
        ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
        ASSERT_TRUE(Until([&] { return coordinator.RequestCancelPrepare(); }));
        auto destroy = Packet(2, {DestroyScene{scene}});
        ASSERT_EQ(coordinator.TrySubmit(destroy).status, PacketSubmitStatus::ACCEPTED);
        coordinator.RequestShutdown();
        EXPECT_EQ(continuations, 0);
        EXPECT_TRUE(Drain(coordinator).empty());
        gate->Release();
        ASSERT_TRUE(Until([&] { return continuations.load() == 1; }));
        EXPECT_EQ(consumers, 0);
        EXPECT_EQ(coordinator.GetState(), CpuCoordinatorState::STOPPING);
        EXPECT_TRUE(Drain(coordinator).empty());
        borrowed->Release();
        coordinator.Join();
        const auto events = Drain(coordinator);
        ASSERT_EQ(events.size(), 3u);
        const auto &cancelled = std::get<CpuPacketCompleted>(events[0]);
        EXPECT_TRUE(cancelled.preparation.cancelled);
        EXPECT_TRUE(cancelled.application.Succeeded());
        EXPECT_FALSE(cancelled.Succeeded());
        EXPECT_EQ(cancelled.preparation.nodes[1].state, PrepareNodeState::SKIPPED);
        EXPECT_FALSE(std::get<CpuPacketCompleted>(events[1]).application.Succeeded());
        EXPECT_TRUE(std::get<CpuCoordinatorStopped>(events[2]).error);
        EXPECT_FALSE(coordinator.RequestCancelPrepare());
        CpuRenderCoordinator rebuilt;
        ASSERT_FALSE(rebuilt.Init({1, 1}));
        auto fresh = Initial();
        ASSERT_EQ(rebuilt.TrySubmit(fresh).status, PacketSubmitStatus::ACCEPTED);
        rebuilt.RequestShutdown();
        rebuilt.Join();
        EXPECT_TRUE(std::get<CpuPacketCompleted>(Drain(rebuilt)[0]).Succeeded());
    }
}

TEST(PrepareCompletion, TerminalPublicationDoesNotRetireContinuationBorrow)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
    auto       frame     = store.Freeze();
    auto       read      = frame->Bind(Reader()).context->Read<RenderTransform>(scene);
    const auto executor  = ContinuationExecutor::Create(1);
    auto       scheduler = ScenePrepareScheduler::Create();
    auto       submitted = scheduler.scheduler->Submit(*frame, {}, {});
    ASSERT_TRUE(submitted.Accepted());
    const auto             token        = submitted.group->GetCompletion();
    int                    calls        = 0;
    CompletionContinuation continuation = [view = std::move(read), &calls](const CompletionResult &result) {
        EXPECT_EQ(result.state, CompletionState::SUCCEEDED);
        view.ForEach([&](RenderEntityId, const RenderTransform &transform) { EXPECT_EQ(transform.world.v[12], 7); });
        ++calls;
    };
    EXPECT_EQ(token->Subscribe(executor->Id(), continuation), CompletionSubscribeResult::ACCEPTED);
    frame.reset();
    EXPECT_TRUE(submitted.group->Wait().Succeeded());
    submitted.group.reset();
    scheduler.scheduler.reset();
    EXPECT_EQ(token->Poll().result.state, CompletionState::SUCCEEDED);
    EXPECT_TRUE(store.IsFrozen());
    EXPECT_FALSE(store.Apply(*Packet(2, {DestroyScene{scene}})).Succeeded());
    executor->Close();
    EXPECT_EQ(executor->Pump(true), ContinuationPumpResult::EXECUTED);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_TRUE(store.Apply(*Packet(2, {DestroyScene{scene}})).Succeeded());
}

TEST(PrepareCompletion, FailureWinsOverCancellationAfterKernelDrain)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
    auto               frame     = store.Freeze();
    auto               scheduler = ScenePrepareScheduler::Create({2});
    auto               gate      = std::make_shared<Gate>();
    std::promise<void> entered;
    auto               ready     = entered.get_future();
    auto               submitted = scheduler.scheduler->Submit(*frame, {Reader()}, [&](const FrozenPrepareContext &context) {
        entered.set_value();
        EXPECT_EQ(gate->future.wait_for(5s), std::future_status::ready);
        context.Fail("failure during cancellation");
    });
    ASSERT_TRUE(submitted.Accepted());
    ScopeExit release{[&] { gate->Release(); }};
    ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
    const auto token = submitted.group->GetCompletion();
    token->RequestCancel();
    EXPECT_EQ(token->Poll().result.state, CompletionState::PENDING);
    EXPECT_TRUE(token->Poll().cancelRequested);
    gate->Release();
    EXPECT_FALSE(submitted.group->Wait().Succeeded());
    EXPECT_FALSE(submitted.group->Wait().cancelled);
    EXPECT_EQ(submitted.group->Poll(), CompletionState::FAILED);
    EXPECT_EQ(submitted.group->Wait().nodes[0].diagnostic, "failure during cancellation");
}

TEST(PrepareCompletion, CancellationCompletionRaceKeepsOneTerminal)
{
    const auto executor = ContinuationExecutor::Create(1);
    for (int i = 0; i < 50; ++i) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(*Initial()).Succeeded());
        auto frame     = store.Freeze();
        auto scheduler = ScenePrepareScheduler::Create({1});
        auto submitted = scheduler.scheduler->Submit(*frame, {Reader()}, [](const FrozenPrepareContext &) {});
        ASSERT_TRUE(submitted.Accepted());
        const auto             token    = submitted.group->GetCompletion();
        int                    calls    = 0;
        CompletionState        terminal = CompletionState::PENDING;
        CompletionContinuation callback = [&](const CompletionResult &result) {
            ++calls;
            terminal = result.state;
        };
        EXPECT_EQ(token->Subscribe(executor->Id(), callback), CompletionSubscribeResult::ACCEPTED);
        token->RequestCancel();
        const auto &result = submitted.group->Wait();
        EXPECT_TRUE(result.Succeeded() || result.cancelled);
        EXPECT_EQ(executor->Pump(true), ContinuationPumpResult::EXECUTED);
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(terminal, token->Poll().result.state);
        EXPECT_EQ(terminal, result.cancelled ? CompletionState::CANCELLED : CompletionState::SUCCEEDED);
        token->RequestCancel();
        EXPECT_EQ(token->Poll().result.state, terminal);
    }
}

TEST(PrepareCompletion, CancelledChunkJoinDiscardsOwnedOutputAndDeferredCommands)
{
    for (const bool serial : {false, true}) {
        CpuSceneStore store;
        auto          packet = Initial();
        packet->commands.emplace_back(CreateRenderEntity{{scene, {1, 1}}, {}});
        ASSERT_TRUE(store.Apply(*packet).Succeeded());
        auto               frame     = store.Freeze();
        auto               scheduler = ScenePrepareScheduler::Create({1, serial ? 0u : 256u});
        auto               gate      = std::make_shared<Gate>();
        std::promise<void> entered;
        auto               ready     = entered.get_future();
        std::atomic_int    kernels   = 0;
        auto               submitted = scheduler.scheduler->Submit(*frame, {Reader()},
                                                                   [&](const FrozenPrepareContext &context) {
                                                         ++kernels;
                                                         EXPECT_EQ(context.Chunk()->index, 0u);
                                                         const std::array bytes{std::byte{7}};
                                                         EXPECT_TRUE(context.Emit(1, bytes));
                                                         EXPECT_TRUE(context.Defer(DestroyRenderEntity{entity}));
                                                         entered.set_value();
                                                         EXPECT_EQ(gate->future.wait_for(5s), std::future_status::ready);
                                                                   },
                                                                   {{1, scene, 1, 16}}, {{1, 1, 1, 1}});
        ASSERT_TRUE(submitted.Accepted());
        ScopeExit release{[&] { gate->Release(); }};
        ASSERT_EQ(ready.wait_for(5s), std::future_status::ready);
        submitted.group->RequestCancel();
        frame.reset();
        EXPECT_TRUE(store.IsFrozen());
        EXPECT_EQ(submitted.group->Poll(), CompletionState::PENDING);
        gate->Release();
        const auto &result = submitted.group->Wait();
        EXPECT_TRUE(result.cancelled);
        EXPECT_FALSE(result.products);
        EXPECT_EQ(result.nodes[0].state, PrepareNodeState::SKIPPED);
        EXPECT_EQ(kernels, 1);
        EXPECT_FALSE(store.IsFrozen());
        EXPECT_NE(store.FindScene(scene)->FindEntity(entity.entity), nullptr);
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
