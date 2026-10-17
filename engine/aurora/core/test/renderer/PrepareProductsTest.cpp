#include "frame/CpuRenderCoordinator.h"
#include "prepare/RenderObjectPrepare.h"
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    constexpr SceneId scene{0, 1};
    RenderEntityRef   Entity(uint32_t index)
    {
        return {scene, {index, 1}};
    }
    FramePacket Packet(uint64_t sequence, std::vector<RenderCommand> commands = {})
    {
        FramePacket packet;
        packet.sequence = sequence;
        packet.commands = std::move(commands);
        return packet;
    }
    FramePacket Initial(uint32_t count = 2)
    {
        auto packet = Packet(1, {CreateScene{scene, {}}});
        for (uint32_t i = 0; i < count; ++i)
            packet.commands.emplace_back(CreateRenderEntity{Entity(i), {}});
        return packet;
    }
    SystemDesc Reader(SystemId id = 1)
    {
        SystemDesc system{id, "ReadTransform"};
        system.accesses = {ComponentAccess<RenderTransform>("scene", SceneAccessOwner(scene), AccessMode::READ)};
        return system;
    }
    bool Emit(const FrozenPrepareContext &context, uint8_t value)
    {
        const std::array payload{static_cast<std::byte>(value)};
        return context.Emit(7, payload);
    }
    std::vector<uint8_t> Values(const PrepareProducts &products)
    {
        std::vector<uint8_t> result;
        for (const auto &record : products.records) {
            auto bytes = products.Read(record);
            EXPECT_EQ(record.channel, 7u);
            EXPECT_EQ(bytes.size(), 1u);
            if (!bytes.empty())
                result.push_back(std::to_integer<uint8_t>(bytes.front()));
        }
        return result;
    }
    const RenderEntityState *Find(const CpuSceneStore &store, uint32_t index)
    {
        return store.FindScene(scene)->FindEntity(Entity(index).entity);
    }
    std::vector<CpuCoordinatorEvent> Drain(CpuRenderCoordinator &coordinator)
    {
        std::array<CpuCoordinatorEvent, 8> buffer;
        const auto                         count = coordinator.PollEvents(buffer);
        return {std::make_move_iterator(buffer.begin()), std::make_move_iterator(buffer.begin() + count)};
    }
} // namespace

TEST(PrepareProducts, MergeOrderAndOwnedPayloadMatchSerialDespiteReversedCompletion)
{
    std::array<std::shared_ptr<const PrepareProducts>, 2> outputs;
    for (size_t serial = 0; serial < 2; ++serial) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        ScenePrepareInit info;
        info.workers = 4;
        if (serial)
            info.maxParallelNodes = 0;
        auto scheduler = ScenePrepareScheduler::Create(info);
        ASSERT_TRUE(scheduler.scheduler);
        auto               frame = store.Freeze();
        std::promise<void> secondFinished;
        auto               secondReady = secondFinished.get_future();
        auto               submitted   = scheduler.scheduler->Submit(*frame, {Reader(2), Reader(1)},
                                                                     [&](const FrozenPrepareContext &context) {
                                                         const auto chunk = context.Chunk()->index;
                                                         if (!serial && context.Id() == 1 && chunk == 0)
                                                             EXPECT_EQ(secondReady.wait_for(5s), std::future_status::ready);
                                                         for (uint8_t ordinal = 0; ordinal < 2; ++ordinal) {
                                                             std::array bytes{static_cast<std::byte>(context.Id() * 10 + chunk * 2 + ordinal)};
                                                             EXPECT_TRUE(context.Emit(7, bytes));
                                                             bytes[0] = std::byte{255};
                                                         }
                                                         if (!serial && context.Id() == 1 && chunk == 1)
                                                             secondFinished.set_value();
                                                                     },
                                                                     {{1, scene, 1}, {2, scene, 1}}, {{1, 2, 2, 0}, {2, 2, 2, 0}});
        ASSERT_TRUE(submitted.Accepted());
        const auto &result = submitted.group->Wait();
        ASSERT_TRUE(result.Succeeded());
        EXPECT_EQ(submitted.group->UsedSerialFallback(), serial != 0);
        outputs[serial] = result.products;
        ASSERT_TRUE(outputs[serial]);
        frame.reset();
        EXPECT_FALSE(store.IsFrozen());
        EXPECT_TRUE(store.Apply(Packet(2, {DestroyScene{scene}})).Succeeded());
    }
    const std::vector<uint8_t> expected{10, 11, 12, 13, 20, 21, 22, 23};
    for (const auto &output : outputs) {
        EXPECT_EQ(Values(*output), expected);
        uint64_t reduction = 0;
        for (auto value : Values(*output))
            reduction = reduction * 31 + value;
        uint64_t reference = 0;
        for (auto value : expected)
            reference = reference * 31 + value;
        EXPECT_EQ(reduction, reference);
        for (size_t i = 0; i < output->records.size(); ++i) {
            EXPECT_EQ(output->records[i].origin.system, i / 4 + 1);
            EXPECT_EQ(output->records[i].origin.chunk, (i / 2) % 2);
            EXPECT_EQ(output->records[i].origin.ordinal, i % 2);
        }
        EXPECT_TRUE(output->Read({{}, 7, std::numeric_limits<size_t>::max(), 1}).empty());
    }
}

TEST(PrepareProducts, SavedContextCannotAppendOffThreadOrAfterCompletion)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto                                scheduler = ScenePrepareScheduler::Create({1});
    auto                                frame     = store.Freeze();
    std::optional<FrozenPrepareContext> saved;
    auto                                submitted = scheduler.scheduler->Submit(*frame, {Reader()},
                                                                                [&](const FrozenPrepareContext &context) {
                                                     saved = context;
                                                     std::thread other([&] {
                                                         EXPECT_FALSE(Emit(context, 9));
                                                         EXPECT_FALSE(context.Defer(DestroyRenderEntity{Entity(0)}));
                                                     });
                                                     other.join();
                                                     EXPECT_TRUE(Emit(context, 3));
                                                     EXPECT_TRUE(context.Defer(DestroyRenderEntity{Entity(0)}));
                                                                                },
                                                                                {}, {{1, 1, 1, 1}});
    ASSERT_TRUE(submitted.Accepted());
    const auto &result = submitted.group->Wait();
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(saved);
    EXPECT_FALSE(Emit(*saved, 4));
    EXPECT_FALSE(saved->Defer(DestroyRenderEntity{Entity(1)}));
    EXPECT_EQ(Values(*result.products), (std::vector<uint8_t>{3}));
    EXPECT_EQ(result.products->deferred.commands.size(), 1u);
    frame.reset();
    EXPECT_TRUE(store.IsFrozen());
    saved.reset();
    EXPECT_FALSE(store.IsFrozen());
}

TEST(PrepareProducts, AnyFailureDiscardsAllOutputsAndFailedSystemCandidates)
{
    for (int failure = 0; failure < 4; ++failure) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        auto             scheduler = ScenePrepareScheduler::Create({3});
        auto             frame     = store.Freeze();
        std::atomic_uint calls     = 0;
        auto             submitted =
            scheduler.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene), Reader(2)},
                                        [&](const FrozenPrepareContext &context) {
                                            ++calls;
                                            EXPECT_TRUE(Emit(context, 1));
                                            EXPECT_TRUE(context.Defer(DestroyRenderEntity{Entity(context.Chunk() ? context.Chunk()->index : 0)}));
                                            if (context.Id() != 1)
                                                return;
                                            PrepareRenderObjects(context, scene);
                                            if (context.Chunk()->index != 0)
                                                return;
                                            if (failure == 0)
                                                context.Fail("Explicit product failure");
                                            if (failure == 1)
                                                EXPECT_FALSE(Emit(context, 2));
                                            if (failure == 2)
                                                EXPECT_FALSE(context.Defer(DestroyRenderEntity{Entity(0)}));
                                            if (failure == 3)
                                                EXPECT_FALSE(context.Emit(0, {}));
                                        },
                                        {{1, scene, 1}}, {{1, 1, 1, 1}, {2, 1, 1, 1}});
        ASSERT_TRUE(submitted.Accepted());
        const auto &result = submitted.group->Wait();
        EXPECT_FALSE(result.Succeeded());
        EXPECT_FALSE(result.products);
        EXPECT_EQ(calls.load(), 3u);
        for (uint32_t i = 0; i < 2; ++i)
            EXPECT_FALSE(store.FindScene(scene)->registry.Get<RenderObjectData>(Find(store, i)->entity)->valid);
    }
}

TEST(PrepareProducts, RejectsInvalidOrOversizedDeclarationsBeforeStartingKernels)
{
    for (int mode = 0; mode < 6; ++mode) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        ScenePrepareInit info;
        info.maxOutputRecords = info.maxOutputBytes = info.maxDeferredCommands = 3;
        auto                             scheduler                             = ScenePrepareScheduler::Create(info);
        auto                             frame                                 = store.Freeze();
        std::vector<PrepareOutputConfig> configs{{1, 1, 1, 1}};
        if (mode == 0)
            configs.push_back(configs[0]);
        if (mode == 1)
            configs[0].system = 7;
        if (mode == 2)
            configs[0].maxRecordsPerTask = 2;
        if (mode == 3)
            configs[0].maxBytesPerTask = 2;
        if (mode == 4)
            configs[0].maxDeferredPerTask = 2;
        if (mode == 5)
            configs[0].maxBytesPerTask = std::numeric_limits<size_t>::max();
        bool ran       = false;
        auto submitted = scheduler.scheduler->Submit(*frame, {Reader()}, [&](const auto &) { ran = true; }, {{1, scene, 1}}, configs);
        EXPECT_FALSE(submitted.Accepted());
        ASSERT_TRUE(submitted.error);
        EXPECT_EQ(submitted.error->code, RendererErrorCode::INVALID_ARGUMENT);
        EXPECT_FALSE(ran);
    }
}

TEST(PrepareProducts, DeferredValidationEnforcesChunkScopeRevisionAndFiniteValues)
{
    for (int mode = 0; mode < 5; ++mode) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        auto scheduler = ScenePrepareScheduler::Create({2});
        auto frame     = store.Freeze();
        auto submitted = scheduler.scheduler->Submit(
            *frame, {Reader()},
            [&](const FrozenPrepareContext &context) {
                if (context.Chunk()->index != 0)
                    return;
                if (mode == 0)
                    EXPECT_FALSE(context.Defer(DestroyRenderEntity{Entity(1)}));
                if (mode == 1)
                    EXPECT_FALSE(context.Defer(CreateRenderEntity{{{1, 1}, {8, 1}}, {}}));
                if (mode == 2)
                    EXPECT_FALSE(context.Defer(UpdateRenderEntity{Entity(0), {}, 0}));
                if (mode == 3) {
                    RenderEntityDesc desc;
                    desc.world.v[0] = std::numeric_limits<float>::quiet_NaN();
                    EXPECT_FALSE(context.Defer(UpdateRenderEntity{Entity(0), desc, 1}));
                }
                if (mode == 4)
                    EXPECT_FALSE(context.Defer(DestroyRenderEntity{Entity(0)}));
            },
            {{1, scene, 1}}, mode == 4 ? std::vector<PrepareOutputConfig>{} : std::vector<PrepareOutputConfig>{{1, 0, 0, 1}});
        ASSERT_TRUE(submitted.Accepted());
        EXPECT_FALSE(submitted.group->Wait().Succeeded());
        EXPECT_FALSE(submitted.group->Wait().products);
    }
}

TEST(PrepareProducts, DeferredActionsWaitForBoundaryAndHostCommandsRunAfterThem)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial(3)).Succeeded());
    auto scheduler = ScenePrepareScheduler::Create({2});
    auto frame     = store.Freeze();
    auto submitted = scheduler.scheduler->Submit(*frame, {Reader()},
                                                 [](const FrozenPrepareContext &context) {
                                                     EXPECT_TRUE(context.Defer(UpdateRenderEntity{Entity(0), {}, 1}));
                                                     EXPECT_TRUE(context.Defer(DestroyRenderEntity{Entity(1)}));
                                                     EXPECT_TRUE(context.Defer(CreateRenderEntity{Entity(3), {}}));
                                                 },
                                                 {}, {{1, 0, 0, 3}});
    ASSERT_TRUE(submitted.Accepted());
    const auto &prepared = submitted.group->Wait();
    ASSERT_TRUE(prepared.Succeeded());
    ASSERT_TRUE(prepared.products);
    EXPECT_EQ(Find(store, 0)->revision, 0u);
    EXPECT_NE(Find(store, 1), nullptr);
    EXPECT_EQ(Find(store, 3), nullptr);
    auto next = Packet(2, {DestroyRenderEntity{Entity(0)}, UpdateRenderEntity{Entity(3), {}, 1}});
    EXPECT_FALSE(store.Apply(next, &prepared.products->deferred).Succeeded());
    frame.reset();
    auto applied = store.Apply(next, &prepared.products->deferred);
    ASSERT_TRUE(applied.Succeeded());
    EXPECT_EQ(applied.deferredSequence, 1u);
    EXPECT_EQ(applied.deferredCommands, 3u);
    EXPECT_EQ(applied.retirements.size(), 2u);
    EXPECT_EQ(Find(store, 0), nullptr);
    EXPECT_EQ(Find(store, 1), nullptr);
    ASSERT_NE(Find(store, 3), nullptr);
    EXPECT_EQ(Find(store, 3)->revision, 1u);
    EXPECT_FALSE(store.Apply(Packet(3), &prepared.products->deferred).Succeeded());
}

TEST(PrepareProducts, BoundaryPreflightIsAtomicAndRevisionGuardsUseEntryState)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    DeferredSceneBatch deferred{1,
                                {{{1, 0, 0}, UpdateRenderEntity{Entity(0), {}, 1}, 0},
                                 {{1, 0, 1}, DestroyRenderEntity{Entity(0)}, 0},
                                 {{1, 0, 2}, CreateRenderEntity{Entity(4), {}}, {}}}};
    auto               invalid = store.Apply(Packet(2, {UpdateRenderEntity{{}, {}, 0}}), &deferred);
    EXPECT_FALSE(invalid.Succeeded());
    EXPECT_TRUE(invalid.retirements.empty());
    EXPECT_EQ(invalid.deferredCommands, 0u);
    EXPECT_EQ(Find(store, 0)->revision, 0u);
    EXPECT_EQ(Find(store, 4), nullptr);
    auto applied = store.Apply(Packet(3), &deferred);
    ASSERT_TRUE(applied.Succeeded());
    EXPECT_TRUE(applied.skipped.empty());
    EXPECT_EQ(applied.retirements.size(), 1u);
    EXPECT_EQ(Find(store, 0), nullptr);
    EXPECT_NE(Find(store, 4), nullptr);
}

TEST(PrepareProducts, DeferredStaleRevisionGenerationAndRetirementAreDiagnosed)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    ASSERT_TRUE(store.Apply(Packet(2, {UpdateRenderEntity{Entity(0), {}, 2}, DestroyRenderEntity{Entity(1)}})).Succeeded());
    DeferredSceneBatch deferred{2,
                                {{{1, 0, 0}, DestroyRenderEntity{Entity(0)}, 0},
                                 {{1, 0, 1}, DestroyRenderEntity{{scene, {0, 2}}}, 2},
                                 {{1, 0, 2}, CreateRenderEntity{{scene, {1, 2}}, {}}, {}}}};
    auto               applied = store.Apply(Packet(3, {UpdateRenderEntity{Entity(0), {}, 1}}), &deferred);
    ASSERT_TRUE(applied.Succeeded());
    ASSERT_EQ(applied.skipped.size(), 4u);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(applied.skipped[i].source, CommandSource::DEFERRED);
        EXPECT_EQ(applied.skipped[i].commandIndex, i);
    }
    EXPECT_EQ(applied.skipped[0].reason, CommandSkipReason::STALE_REVISION);
    EXPECT_EQ(applied.skipped[1].reason, CommandSkipReason::STALE_HANDLE);
    EXPECT_EQ(applied.skipped[2].reason, CommandSkipReason::RETIREMENT_PENDING);
    EXPECT_EQ(applied.skipped[3].source, CommandSource::PACKET);
    EXPECT_TRUE(store.AcknowledgeRetirement(Entity(1)));
    deferred.sourceSequence = 3;
    deferred.commands.erase(deferred.commands.begin(), deferred.commands.begin() + 2);
    ASSERT_TRUE(store.Apply(Packet(4), &deferred).Succeeded());
    EXPECT_NE(store.FindScene(scene)->FindEntity({1, 2}), nullptr);
}

TEST(PrepareProducts, CoordinatorRetainsDeferredAcrossInvalidPacketAndConsumesOnce)
{
    std::vector<size_t>  counts;
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({3, 3}, [&](const FrozenCpuFrame &frame) {
        CpuPrepareProgram program;
        program.systems     = {Reader()};
        program.outputs     = {{1, 1, 1, 1}};
        const auto sequence = frame.Sequence();
        program.kernel      = [&, sequence](const FrozenPrepareContext &context) {
            counts.push_back(context.Read<RenderTransform>(scene).Size());
            EXPECT_TRUE(Emit(context, static_cast<uint8_t>(sequence)));
            if (sequence == 1)
                EXPECT_TRUE(context.Defer(DestroyRenderEntity{Entity(0)}));
        };
        return program;
    }));
    std::array packets{Initial(), Packet(2, {UpdateRenderEntity{{}, {}, 0}}), Packet(3)};
    for (auto &packet : packets) {
        auto owned = std::make_unique<FramePacket>(std::move(packet));
        ASSERT_EQ(coordinator.TrySubmit(owned).status, PacketSubmitStatus::ACCEPTED);
    }
    coordinator.RequestShutdown();
    coordinator.Join();
    EXPECT_EQ(counts, (std::vector<size_t>{2, 1}));
    auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 4u);
    const auto &first = std::get<CpuPacketCompleted>(events[0]);
    EXPECT_TRUE(first.Succeeded());
    EXPECT_EQ(Values(*first.preparation.products), (std::vector<uint8_t>{1}));
    EXPECT_FALSE(std::get<CpuPacketCompleted>(events[1]).application.Succeeded());
    const auto &third = std::get<CpuPacketCompleted>(events[2]);
    EXPECT_TRUE(third.Succeeded());
    EXPECT_EQ(third.application.deferredSequence, 1u);
    EXPECT_EQ(third.application.deferredCommands, 1u);
    EXPECT_EQ(std::get<CpuCoordinatorStopped>(events[3]).discardedDeferredCommands, 0u);
}

TEST(PrepareProducts, ShutdownReportsUnappliedCommandsAndFailedPreparePublishesNone)
{
    for (bool fail : {false, true}) {
        CpuRenderCoordinator coordinator;
        ASSERT_FALSE(coordinator.Init({1, 1}, [fail](const FrozenCpuFrame &) {
            CpuPrepareProgram program;
            program.systems = {Reader()};
            program.outputs = {{1, 0, 0, 1}};
            program.kernel  = [fail](const FrozenPrepareContext &context) {
                EXPECT_TRUE(context.Defer(DestroyRenderEntity{Entity(0)}));
                if (fail)
                    context.Fail("Discard deferred request");
            };
            return program;
        }));
        auto packet = std::make_unique<FramePacket>(Initial());
        ASSERT_EQ(coordinator.TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
        coordinator.RequestShutdown();
        coordinator.Join();
        auto events = Drain(coordinator);
        ASSERT_EQ(events.size(), 2u);
        const auto &completed = std::get<CpuPacketCompleted>(events[0]);
        EXPECT_EQ(completed.Succeeded(), !fail);
        EXPECT_EQ(completed.preparation.products != nullptr, !fail);
        const auto &stopped = std::get<CpuCoordinatorStopped>(events[1]);
        EXPECT_EQ(stopped.error.has_value(), fail);
        EXPECT_EQ(stopped.discardedDeferredCommands, fail ? 0u : 1u);
    }
}

TEST(PrepareProducts, FreshHostInputOverridesDeferredRevisionButStaleInputDoesNot)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    ASSERT_TRUE(store.Apply(Packet(2, {UpdateRenderEntity{Entity(0), {}, 2}, UpdateRenderEntity{Entity(1), {}, 2}})).Succeeded());
    RenderEntityDesc deferredValue;
    deferredValue.world.v[12] = 50;
    RenderEntityDesc hostValue;
    hostValue.world.v[12] = 7;
    DeferredSceneBatch deferred{
        2, {{{1, 0, 0}, UpdateRenderEntity{Entity(0), deferredValue, 50}, 2}, {{1, 0, 1}, UpdateRenderEntity{Entity(1), deferredValue, 50}, 2}}};
    auto applied = store.Apply(Packet(3, {UpdateRenderEntity{Entity(0), hostValue, 2}, UpdateRenderEntity{Entity(0), hostValue, 3},
                                          UpdateRenderEntity{Entity(0), deferredValue, 3}, UpdateRenderEntity{Entity(1), hostValue, 2}}),
                               &deferred);
    ASSERT_TRUE(applied.Succeeded());
    ASSERT_EQ(applied.skipped.size(), 3u);
    EXPECT_EQ(applied.skipped[0].commandIndex, 0u);
    EXPECT_EQ(applied.skipped[1].commandIndex, 2u);
    EXPECT_EQ(applied.skipped[2].commandIndex, 3u);
    EXPECT_EQ(Find(store, 0)->revision, 3u);
    EXPECT_EQ(Find(store, 1)->revision, 50u);
    EXPECT_EQ(store.FindScene(scene)->registry.Get<RenderTransform>(Find(store, 0)->entity)->world.v[12], 7);
    EXPECT_EQ(store.FindScene(scene)->registry.Get<RenderTransform>(Find(store, 1)->entity)->world.v[12], 50);
}

TEST(PrepareProducts, CoordinatorWaitsForEscapedReadBeforeDeferredApply)
{
    std::promise<void>                             captured;
    auto                                           ready = captured.get_future();
    std::optional<FrozenReadView<RenderTransform>> borrowed;
    std::atomic_uint                               secondRows = 99;
    CpuRenderCoordinator                           coordinator;
    ASSERT_FALSE(coordinator.Init({2, 2}, [&](const FrozenCpuFrame &frame) {
        CpuPrepareProgram program;
        program.systems = {Reader()};
        program.outputs = {{1, 0, 0, 1}};
        program.kernel  = [&, sequence = frame.Sequence()](const FrozenPrepareContext &context) {
            if (sequence == 1) {
                borrowed = context.Read<RenderTransform>(scene);
                EXPECT_TRUE(context.Defer(DestroyRenderEntity{Entity(0)}));
                captured.set_value();
            } else {
                secondRows = static_cast<unsigned>(context.Read<RenderTransform>(scene).Size());
            }
        };
        return program;
    }));
    auto first  = std::make_unique<FramePacket>(Initial());
    auto second = std::make_unique<FramePacket>(Packet(2));
    EXPECT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    EXPECT_EQ(coordinator.TrySubmit(second).status, PacketSubmitStatus::ACCEPTED);
    const bool received = ready.wait_for(5s) == std::future_status::ready;
    EXPECT_TRUE(received);
    coordinator.RequestShutdown();
    if (received) {
        EXPECT_EQ(borrowed->Size(), 2u);
        EXPECT_EQ(secondRows.load(), 99u);
        EXPECT_TRUE(Drain(coordinator).empty());
        borrowed.reset();
    }
    coordinator.Join();
    EXPECT_EQ(secondRows.load(), 1u);
    const auto events = Drain(coordinator);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(std::get<CpuPacketCompleted>(events[1]).application.deferredCommands, 1u);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
