#include "frame/CpuRenderCoordinator.h"
#include "prepare/RenderObjectPrepare.h"
#include <core/async/ThreadPool.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    constexpr SceneId scene{0, 1};
    RenderEntityId    Entity(uint32_t index)
    {
        return {index * 3 + 2, 1};
    }
    FramePacket Initial(uint32_t count)
    {
        FramePacket packet;
        packet.sequence = 1;
        packet.commands.emplace_back(CreateScene{scene, {}});
        for (uint32_t i = count; i != 0; --i) {
            RenderEntityDesc desc;
            desc.world.v[12] = static_cast<float>(i);
            packet.commands.emplace_back(CreateRenderEntity{{scene, Entity(i - 1)}, desc});
        }
        return packet;
    }
    SystemDesc Reader(SystemId id = 2)
    {
        SystemDesc desc{id, "ReadObjects"};
        desc.accesses = {ComponentAccess<RenderObjectData>("scene", SceneAccessOwner(scene), AccessMode::READ)};
        return desc;
    }
    const RenderObjectData &Object(const CpuSceneStore &store, uint32_t index)
    {
        const auto *owner = store.FindScene(scene);
        return *owner->registry.Get<RenderObjectData>(owner->FindEntity(Entity(index))->entity);
    }
    struct Gate {
        void Enter()
        {
            std::unique_lock lock(mutex);
            ++entered;
            changed.notify_all();
            EXPECT_TRUE(changed.wait_for(lock, 5s, [&] { return released; }));
        }
        bool Await(uint32_t count)
        {
            std::unique_lock lock(mutex);
            return changed.wait_for(lock, 5s, [&] { return entered >= count; });
        }
        void Release()
        {
            std::lock_guard lock(mutex);
            released = true;
            changed.notify_all();
        }
        std::mutex              mutex;
        std::condition_variable changed;
        uint32_t                entered  = 0;
        bool                    released = false;
    };
} // namespace

TEST(ChunkPrepare, RangesOverlapInTimeWithPrivateScratchAndPublishAtSystemJoin)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial(9)).Succeeded());
    auto created = ScenePrepareScheduler::Create({2});
    ASSERT_TRUE(created.scheduler);
    auto                                 frame = store.Freeze();
    Gate                                 gate;
    std::array<std::thread::id, 3>       threads;
    std::array<uintptr_t, 3>             scratchAddresses{};
    std::array<std::vector<uint32_t>, 3> rows;
    std::optional<FrozenPrepareContext>  saved;
    std::atomic_bool                     readerRan = false;
    auto                                 submitted = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene), Reader()},
                                                                               [&](const FrozenPrepareContext &context) {
                                                   if (context.Id() == 2) {
                                                       EXPECT_FALSE(context.Chunk());
                                                       size_t index = 0;
                                                       context.Read<RenderObjectData>(scene).ForEach([&](auto id, const auto &data) {
                                                           EXPECT_EQ(id, Entity(static_cast<uint32_t>(index)));
                                                           EXPECT_TRUE(data.valid);
                                                           EXPECT_EQ(data.value.world.v[12], static_cast<float>(++index));
                                                           EXPECT_EQ(data.preparedSequence, 1u);
                                                       });
                                                       EXPECT_EQ(index, 9u);
                                                       readerRan.store(true);
                                                       return;
                                                   }
                                                   const auto chunk = *context.Chunk();
                                                   EXPECT_EQ(chunk.total, 3u);
                                                   EXPECT_EQ(chunk.first, chunk.index * 3);
                                                   EXPECT_EQ(chunk.count, 3u);
                                                   threads[chunk.index] = std::this_thread::get_id();
                                                   auto scratch         = context.Scratch();
                                                   ASSERT_EQ(scratch.size(), 16u);
                                                   scratchAddresses[chunk.index] = reinterpret_cast<uintptr_t>(scratch.data());
                                                   for (auto &value : scratch) {
                                                       EXPECT_EQ(value, std::byte{});
                                                       value = static_cast<std::byte>(chunk.index + 1);
                                                   }
                                                   context.Read<RenderTransform>(scene).ForEach(
                                                       [&](auto id, const auto &) { rows[chunk.index].push_back(id.index); });
                                                   EXPECT_EQ(context.Write<RenderObjectData>(scene).Size(), chunk.count);
                                                   PrepareRenderObjects(context, scene);
                                                   if (chunk.index == 0) {
                                                       saved = context;
                                                       std::thread other([&] { EXPECT_TRUE(context.Scratch().empty()); });
                                                       other.join();
                                                   }
                                                   if (chunk.index < 2)
                                                       gate.Enter();
                                                   for (auto value : scratch)
                                                       EXPECT_EQ(value, static_cast<std::byte>(chunk.index + 1));
                                                                               },
                                                                               {{1, scene, 3, 16}});
    ASSERT_TRUE(submitted.Accepted());
    EXPECT_TRUE(gate.Await(2));
    EXPECT_EQ(submitted.group->Poll(), CompletionState::PENDING);
    EXPECT_FALSE(readerRan.load());
    for (uint32_t i = 0; i < 9; ++i)
        EXPECT_FALSE(Object(store, i).valid);
    gate.Release();
    EXPECT_TRUE(submitted.group->Wait().Succeeded());
    EXPECT_TRUE(readerRan.load());
    EXPECT_NE(threads[0], threads[1]);
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(rows[i], (std::vector<uint32_t>{Entity(i * 3).index, Entity(i * 3 + 1).index, Entity(i * 3 + 2).index}));
        for (size_t j = i + 1; j < 3; ++j)
            EXPECT_NE(scratchAddresses[i], scratchAddresses[j]);
    }
    ASSERT_TRUE(saved);
    EXPECT_TRUE(saved->Scratch().empty());
    frame.reset();
    EXPECT_TRUE(store.IsFrozen());
    saved.reset();
    EXPECT_FALSE(store.IsFrozen());
}

TEST(ChunkPrepare, FailedChunksDiscardWholeSystemAndChooseDiagnosticInChunkOrder)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial(8)).Succeeded());
    auto created = ScenePrepareScheduler::Create({3});
    auto frame   = store.Freeze();
    auto first   = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene)},
                                             [](const FrozenPrepareContext &context) { PrepareRenderObjects(context, scene); });
    ASSERT_TRUE(first.Accepted());
    ASSERT_TRUE(first.group->Wait().Succeeded());
    frame.reset();
    FramePacket update;
    update.sequence = 2;
    for (uint32_t i = 0; i < 8; ++i) {
        RenderEntityDesc desc;
        desc.world.v[12] = 99;
        update.commands.emplace_back(UpdateRenderEntity{{scene, Entity(i)}, desc, 1});
    }
    ASSERT_TRUE(store.Apply(update).Succeeded());
    frame = store.Freeze();
    Gate                 slowFirst, slowFailure;
    std::promise<void>   secondFailed;
    auto                 secondReady = secondFailed.get_future();
    std::atomic_uint32_t calls       = 0;
    std::atomic_bool     independent = false;
    auto submitted = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene), Reader(), SystemDesc{3, "Independent"}},
                                               [&](const FrozenPrepareContext &context) {
                                                   if (context.Id() == 3) {
                                                       independent.store(true);
                                                       return;
                                                   }
                                                   if (context.Id() == 2) {
                                                       ADD_FAILURE() << "Consumer of failed chunks ran";
                                                       return;
                                                   }
                                                   ++calls;
                                                   PrepareRenderObjects(context, scene);
                                                   switch (context.Chunk()->index) {
                                                   case 0: slowFirst.Enter(); break;
                                                   case 1:
                                                       slowFailure.Enter();
                                                       context.Fail("chunk 1 failed");
                                                       break;
                                                   case 2:
                                                       secondFailed.set_value();
                                                       context.Fail("chunk 2 failed");
                                                       break;
                                                   default: break;
                                                   }
                                               },
                                               {{1, scene, 2}, {2, scene, 2}});
    ASSERT_TRUE(submitted.Accepted());
    EXPECT_TRUE(slowFirst.Await(1));
    EXPECT_TRUE(slowFailure.Await(1));
    EXPECT_EQ(secondReady.wait_for(5s), std::future_status::ready);
    EXPECT_EQ(submitted.group->Poll(), CompletionState::PENDING);
    slowFailure.Release();
    slowFirst.Release();
    const auto &result = submitted.group->Wait();
    ASSERT_EQ(result.nodes.size(), 3u);
    EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
    EXPECT_EQ(result.nodes[0].diagnostic, "chunk 1 failed");
    EXPECT_EQ(result.nodes[1].state, PrepareNodeState::SKIPPED);
    EXPECT_EQ(result.nodes[2].state, PrepareNodeState::SUCCEEDED);
    EXPECT_EQ(calls.load(), 4u);
    EXPECT_TRUE(independent.load());
    frame.reset();
    EXPECT_FALSE(store.IsFrozen());
    for (uint32_t i = 0; i < 8; ++i) {
        EXPECT_EQ(Object(store, i).value.world.v[12], static_cast<float>(i + 1));
        EXPECT_EQ(Object(store, i).preparedSequence, 1u);
        EXPECT_EQ(Object(store, i).sourceRevision, 0u);
        EXPECT_FALSE(Object(store, i).valid);
    }
}

TEST(ChunkPrepare, SuccessivePartitionedWritersAndReadersRespectWholeSystemDependencies)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial(11)).Succeeded());
    auto       created  = ScenePrepareScheduler::Create({3});
    auto       frame    = store.Freeze();
    auto       producer = DescribeRenderObjectPrepare(20, scene);
    SystemDesc modify{10, "Modify"};
    modify.accesses              = {ComponentAccess<RenderObjectData>("scene", SceneAccessOwner(scene), AccessMode::WRITE)};
    modify.after                 = {20};
    auto read                    = Reader(1);
    read.after                   = {10};
    std::atomic_uint32_t visited = 0;
    auto                 submitted =
        created.scheduler->Submit(*frame, {read, modify, producer},
                                  [&](const FrozenPrepareContext &context) {
                                      if (context.Id() == 20)
                                          PrepareRenderObjects(context, scene);
                                      else if (context.Id() == 10)
                                          context.Write<RenderObjectData>(scene).ForEach([](auto, auto &value) { value.world.v[12] += 10; });
                                      else
                                          context.Read<RenderObjectData>(scene).ForEach([&](auto id, const auto &data) {
                                              EXPECT_TRUE(data.valid);
                                              EXPECT_EQ(data.value.world.v[12], static_cast<float>((id.index - 2) / 3 + 11));
                                              ++visited;
                                          });
                                  },
                                  {{20, scene, 4}, {10, scene, 2}, {1, scene, 3}});
    ASSERT_TRUE(submitted.Accepted());
    EXPECT_TRUE(submitted.group->Wait().Succeeded());
    EXPECT_EQ(visited.load(), 11u);
}

TEST(ChunkPrepare, SerialFallbackPreservesPartitionsScratchValuesAndFailureSemantics)
{
    for (bool fail : {false, true}) {
        std::array<std::vector<float>, 2>            observations;
        std::array<std::vector<PrepareNodeState>, 2> states;
        for (uint32_t mode = 0; mode < 2; ++mode) {
            CpuSceneStore store;
            ASSERT_TRUE(store.Apply(Initial(13)).Succeeded());
            auto                 created = ScenePrepareScheduler::Create({2, mode == 0 ? 0u : 256u});
            auto                 frame   = store.Freeze();
            std::atomic_uint32_t chunks  = 0;
            const auto           caller  = std::this_thread::get_id();
            std::mutex           serialMutex;
            std::thread::id      serialThread;
            auto                 submitted = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene), Reader()},
                                                                       [&](const FrozenPrepareContext &context) {
                                                           EXPECT_NE(std::this_thread::get_id(), caller);
                                                           if (mode == 0) {
                                                               std::lock_guard lock(serialMutex);
                                                               if (serialThread == std::thread::id{})
                                                                   serialThread = std::this_thread::get_id();
                                                               EXPECT_EQ(std::this_thread::get_id(), serialThread);
                                                           }
                                                           if (context.Id() == 1) {
                                                               ++chunks;
                                                               EXPECT_EQ(context.Chunk()->total, 4u);
                                                               EXPECT_EQ(context.Scratch().size(), 12u);
                                                               PrepareRenderObjects(context, scene);
                                                               if (fail && context.Chunk()->index == 2)
                                                                   context.Fail("same failure");
                                                           }
                                                                       },
                                                                       {{1, scene, 4, 12}});
            ASSERT_TRUE(submitted.Accepted());
            EXPECT_EQ(submitted.group->UsedSerialFallback(), mode == 0);
            const auto &result = submitted.group->Wait();
            EXPECT_EQ(result.Succeeded(), !fail);
            for (const auto &node : result.nodes)
                states[mode].push_back(node.state);
            EXPECT_EQ(chunks.load(), 4u);
            for (uint32_t i = 0; i < 13; ++i) {
                const auto &object = Object(store, i);
                observations[mode].push_back(object.value.world.v[12]);
                EXPECT_EQ(object.valid, !fail);
                EXPECT_EQ(object.preparedSequence, fail ? 0u : 1u);
            }
        }
        EXPECT_EQ(observations[0], observations[1]);
        EXPECT_EQ(states[0], states[1]);
    }
}

TEST(ChunkPrepare, InvalidDeclarationsAndHardBudgetsRejectBeforeAnyKernel)
{
    for (int mode = 0; mode < 10; ++mode) {
        CpuSceneStore store;
        auto          packet = Initial(4);
        packet.commands.emplace_back(CreateScene{{1, 1}, {}});
        ASSERT_TRUE(store.Apply(packet).Succeeded());
        ScenePrepareInit info;
        if (mode == 4)
            info.maxTaskNodes = 4;
        if (mode == 5)
            info.maxScratchBytes = 31;
        if (mode == 8)
            info.maxScratchBytes = std::numeric_limits<size_t>::max();
        if (mode == 9)
            info.maxScratchBytes = 48;
        auto                            created = ScenePrepareScheduler::Create(info);
        auto                            frame   = store.Freeze();
        auto                            desc    = DescribeRenderObjectPrepare(1, scene);
        std::vector<ElementWisePrepare> options{{1, scene, 1, 8}};
        if (mode == 0)
            options[0].rowsPerChunk = 0;
        if (mode == 1)
            options[0].system = 99;
        if (mode == 2)
            options.push_back(options[0]);
        if (mode == 3)
            options[0].scene = {0, 2};
        if (mode == 6)
            desc.accesses.push_back(ResourceAccess("shared", {}, "output", AccessMode::WRITE));
        if (mode == 7)
            desc.accesses.push_back(ComponentAccess<RenderTransform>("scene", SceneAccessOwner({1, 1}), AccessMode::READ));
        if (mode == 8)
            options[0].scratchBytes = std::numeric_limits<size_t>::max() / 2 + 1;
        std::vector<SystemDesc> systems{desc};
        if (mode == 9) {
            systems.push_back(DescribeRenderObjectPrepare(2, scene));
            options.push_back({2, scene, 1, 8});
        }
        auto submitted = created.scheduler->Submit(*frame, std::move(systems), [](const auto &) { ADD_FAILURE(); }, std::move(options));
        EXPECT_FALSE(submitted.Accepted());
        ASSERT_TRUE(submitted.error);
        EXPECT_EQ(submitted.error->code, RendererErrorCode::INVALID_ARGUMENT);
        frame.reset();
        EXPECT_FALSE(store.IsFrozen());
        for (uint32_t i = 0; i < 4; ++i)
            EXPECT_FALSE(Object(store, i).valid);
    }
}

TEST(ChunkPrepare, EmptyAndSingleRangeExecuteOnceAndDoNotOverflow)
{
    for (uint32_t count : {0u, 1u, 7u}) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial(count)).Succeeded());
        auto     created   = ScenePrepareScheduler::Create({1});
        auto     frame     = store.Freeze();
        uint32_t calls     = 0;
        auto     submitted = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene)},
                                                       [&](const FrozenPrepareContext &context) {
                                                       ++calls;
                                                       EXPECT_EQ(context.Chunk()->total, 1u);
                                                       EXPECT_EQ(context.Chunk()->first, 0u);
                                                       EXPECT_EQ(context.Chunk()->count, count);
                                                       EXPECT_EQ(context.Read<RenderTransform>(scene).Size(), count);
                                                       PrepareRenderObjects(context, scene);
                                                       },
                                                       {{1, scene, std::numeric_limits<size_t>::max()}});
        ASSERT_TRUE(submitted.Accepted());
        EXPECT_TRUE(submitted.group->Wait().Succeeded());
        EXPECT_EQ(calls, 1u);
    }
}

TEST(ChunkPrepare, QueueBudgetCountsJoinNodeAndFallsBackAboveBoundary)
{
    for (uint32_t extra : {0u, 1u}) {
        const auto    rows = ThreadPool::GetLocalQueueCapacity() - 1 + extra;
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial(rows)).Succeeded());
        auto                 created   = ScenePrepareScheduler::Create({1, 10000});
        auto                 frame     = store.Freeze();
        std::atomic_uint32_t calls     = 0;
        auto                 submitted = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene)},
                                                                   [&](const FrozenPrepareContext &context) {
                                                       ++calls;
                                                       PrepareRenderObjects(context, scene);
                                                                   },
                                                                   {{1, scene, 1}});
        ASSERT_TRUE(submitted.Accepted());
        EXPECT_TRUE(submitted.group->Wait().Succeeded());
        EXPECT_EQ(submitted.group->UsedSerialFallback(), extra != 0);
        EXPECT_EQ(calls.load(), rows);
        for (uint32_t i = 0; i < rows; ++i)
            EXPECT_TRUE(Object(store, i).valid);
    }
}

TEST(ChunkPrepare, RestrictedViewsCannotExpandAndRetainedInputPinsOnlyItsBoundRows)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial(5)).Succeeded());
    auto                            created = ScenePrepareScheduler::Create({2});
    auto                            frame   = store.Freeze();
    FrozenReadView<RenderTransform> held;
    auto                            submitted = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene)},
                                                                          [&](const FrozenPrepareContext &context) {
                                                   PrepareRenderObjects(context, scene);
                                                   if (context.Chunk()->index == 0) {
                                                       held = context.Read<RenderTransform>(scene);
                                                       context.Write<RenderObjectData>(scene).Slice(0, 3);
                                                   }
                                                                          },
                                                                          {{1, scene, 2}});
    ASSERT_TRUE(submitted.Accepted());
    EXPECT_FALSE(submitted.group->Wait().Succeeded());
    EXPECT_EQ(held.Size(), 2u);
    std::vector<uint32_t> ids;
    held.ForEach([&](auto id, const auto &) { ids.push_back(id.index); });
    EXPECT_EQ(ids, (std::vector<uint32_t>{Entity(0).index, Entity(1).index}));
    frame.reset();
    created.scheduler.reset();
    EXPECT_TRUE(store.IsFrozen());
    held = {};
    EXPECT_FALSE(store.IsFrozen());
    for (uint32_t i = 0; i < 5; ++i)
        EXPECT_FALSE(Object(store, i).valid);
}

TEST(ChunkPrepare, CoordinatorShutdownDrainsChunksThenAppliesAcceptedPacket)
{
    Gate                 gate;
    std::atomic_uint32_t chunks = 0;
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({2, 2}, [&](const FrozenCpuFrame &) {
        return CpuPrepareProgram{{DescribeRenderObjectPrepare(1, scene)},
                                 [&](const FrozenPrepareContext &context) {
                                     ++chunks;
                                     PrepareRenderObjects(context, scene);
                                     if (context.Sequence() == 1)
                                         gate.Enter();
                                 },
                                 {{1, scene, 2}}};
    }));
    auto first = std::make_unique<FramePacket>(Initial(4));
    ASSERT_EQ(coordinator.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    EXPECT_TRUE(gate.Await(2));
    auto second      = std::make_unique<FramePacket>();
    second->sequence = 2;
    second->commands.emplace_back(DestroyRenderEntity{{scene, Entity(0)}});
    EXPECT_EQ(coordinator.TrySubmit(second).status, PacketSubmitStatus::ACCEPTED);
    coordinator.RequestShutdown();
    EXPECT_EQ(coordinator.GetState(), CpuCoordinatorState::STOPPING);
    std::array<CpuCoordinatorEvent, 3> events;
    EXPECT_EQ(coordinator.PollEvents(events), 0u);
    gate.Release();
    coordinator.Join();
    ASSERT_EQ(coordinator.PollEvents(events), 3u);
    EXPECT_TRUE(std::get<CpuPacketCompleted>(events[0]).Succeeded());
    EXPECT_TRUE(std::get<CpuPacketCompleted>(events[1]).Succeeded());
    EXPECT_EQ(std::get<CpuPacketCompleted>(events[1]).application.retirements.size(), 1u);
    EXPECT_FALSE(std::get<CpuCoordinatorStopped>(events[2]).error);
    EXPECT_EQ(chunks.load(), 4u);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
