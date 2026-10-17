#include "frame/CpuRenderCoordinator.h"
#include "prepare/RenderObjectPrepare.h"
#include <core/async/ThreadPool.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    constexpr SceneId scene{0, 1};
    constexpr SceneId secondScene{1, 1};
    FramePacket       Initial(uint64_t sequence = 1)
    {
        FramePacket packet;
        packet.sequence = sequence;
        for (auto id : {scene, secondScene}) {
            RenderEntityDesc entity;
            entity.world.v[12] = static_cast<float>(id.index + 4);
            packet.commands.emplace_back(CreateScene{id, {}});
            packet.commands.emplace_back(CreateRenderEntity{{id, {0, 1}}, entity});
        }
        return packet;
    }
    SystemDesc Reader(SystemId id, SceneId owner)
    {
        SystemDesc result{id, "ReadObjects"};
        result.accesses = {ComponentAccess<RenderObjectData>("scene", SceneAccessOwner(owner), AccessMode::READ)};
        return result;
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

TEST(ParallelPrepare, IndependentScenesOverlapAndConsumersObservePublishedData)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto created = ScenePrepareScheduler::Create({2});
    ASSERT_TRUE(created.scheduler);
    auto                           frame = store.Freeze();
    Gate                           gate;
    std::array<std::thread::id, 2> threads;
    std::array<float, 3>           values{};
    auto                           submission = created.scheduler->Submit(
        *frame,
        {DescribeRenderObjectPrepare(1, scene), DescribeRenderObjectPrepare(2, secondScene), Reader(3, scene), Reader(4, scene),
         Reader(5, secondScene)},
        [&](const FrozenPrepareContext &context) {
            if (context.Id() <= 2) {
                threads[context.Id() - 1] = std::this_thread::get_id();
                gate.Enter();
                PrepareRenderObjects(context, context.Id() == 1 ? scene : secondScene);
            } else {
                context.Read<RenderObjectData>(context.Id() == 5 ? secondScene : scene).ForEach([&](auto, const auto &object) {
                    EXPECT_TRUE(object.valid);
                    values[context.Id() - 3] = object.value.world.v[12];
                });
            }
        });
    ASSERT_TRUE(submission.Accepted());
    EXPECT_TRUE(gate.Await(2));
    EXPECT_EQ(submission.group->Poll(), CompletionState::PENDING);
    EXPECT_FALSE(created.scheduler->Submit(*frame, {}, {}).Accepted());
    gate.Release();
    EXPECT_TRUE(submission.group->Wait().Succeeded());
    EXPECT_NE(threads[0], threads[1]);
    EXPECT_NE(threads[0], std::this_thread::get_id());
    EXPECT_EQ(values, (std::array<float, 3>{4, 4, 5}));
    EXPECT_FALSE(submission.group->UsedSerialFallback());
}

TEST(ParallelPrepare, FailureDiscardsCandidatesAndWaitsForIndependentNode)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto             created = ScenePrepareScheduler::Create({2});
    auto             frame   = store.Freeze();
    Gate             gate;
    std::atomic_bool failed = false;
    auto submission = created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene), Reader(2, scene), SystemDesc{3, "Independent"}},
                                                [&](const FrozenPrepareContext &context) {
                                                    if (context.Id() == 1) {
                                                        PrepareRenderObjects(context, scene);
                                                        EXPECT_TRUE(context.Fail("injected failure"));
                                                        failed.store(true);
                                                    } else if (context.Id() == 2) {
                                                        ADD_FAILURE() << "Failed writer's consumer ran";
                                                    } else {
                                                        gate.Enter();
                                                    }
                                                });
    ASSERT_TRUE(submission.Accepted());
    EXPECT_TRUE(gate.Await(1));
    EXPECT_EQ(submission.group->Poll(), CompletionState::PENDING);
    gate.Release();
    const auto &result = submission.group->Wait();
    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(failed.load());
    EXPECT_EQ(result.nodes[0].diagnostic, "injected failure");
    EXPECT_EQ(result.nodes[1].state, PrepareNodeState::SKIPPED);
    EXPECT_EQ(result.nodes[2].state, PrepareNodeState::SUCCEEDED);
    frame.reset();
    EXPECT_FALSE(store.IsFrozen());
    const auto *state = store.FindScene(scene);
    EXPECT_FALSE(state->registry.Get<RenderObjectData>(state->FindEntity({0, 1})->entity)->valid);
}

TEST(ParallelPrepare, QueueBoundaryFanOutAndOversizeSerialFallbackBothFinish)
{
    for (uint32_t workers : {1u, 3u}) {
        auto created = ScenePrepareScheduler::Create({workers, 100000});
        ASSERT_TRUE(created.scheduler);
        for (uint32_t extra : {0u, 1u}) {
            CpuSceneStore store;
            ASSERT_TRUE(store.Apply(Initial()).Succeeded());
            auto                    frame = store.Freeze();
            const uint32_t          count = workers * ThreadPool::GetLocalQueueCapacity() + extra;
            std::vector<SystemDesc> systems;
            for (uint32_t i = 0; i < count; ++i) {
                SystemDesc system{i + 1, "FanOut"};
                if (i)
                    system.after = {1};
                systems.push_back(std::move(system));
            }
            std::atomic_uint32_t calls  = 0;
            const auto           caller = std::this_thread::get_id();
            std::mutex           serialMutex;
            std::thread::id      serialThread;
            auto                 submission = created.scheduler->Submit(*frame, std::move(systems), [&](const auto &) {
                ++calls;
                EXPECT_NE(std::this_thread::get_id(), caller);
                if (extra) {
                    std::lock_guard lock(serialMutex);
                    if (serialThread == std::thread::id{})
                        serialThread = std::this_thread::get_id();
                    EXPECT_EQ(std::this_thread::get_id(), serialThread);
                }
            });
            ASSERT_TRUE(submission.Accepted());
            EXPECT_TRUE(submission.group->Wait().Succeeded());
            EXPECT_EQ(submission.group->UsedSerialFallback(), extra != 0);
            EXPECT_EQ(calls.load(), count);
        }
    }
}

TEST(ParallelPrepare, SerialAndParallelPlansProduceEqualValuesAndStamps)
{
    std::array<std::array<uint64_t, 3>, 2> observations{};
    for (uint32_t mode = 0; mode < 2; ++mode) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        auto created = ScenePrepareScheduler::Create({2, mode == 0 ? 0u : 256u});
        auto frame   = store.Freeze();
        auto submission =
            created.scheduler->Submit(*frame, {DescribeRenderObjectPrepare(1, scene), Reader(2, scene)}, [&](const FrozenPrepareContext &context) {
                if (context.Id() == 1)
                    PrepareRenderObjects(context, scene);
                else
                    context.Read<RenderObjectData>(scene).ForEach([&](auto, const auto &object) {
                        observations[mode] = {static_cast<uint64_t>(object.value.world.v[12]), object.sourceRevision, object.preparedSequence};
                    });
            });
        ASSERT_TRUE(submission.Accepted());
        EXPECT_TRUE(submission.group->Wait().Succeeded());
    }
    EXPECT_EQ(observations[0], observations[1]);
}

TEST(ParallelPrepare, InvalidPlansBindingsAndRowBudgetStartNoKernels)
{
    auto invalid = ScenePrepareScheduler::Create({0});
    EXPECT_FALSE(invalid.scheduler);
    ASSERT_TRUE(invalid.error);
    EXPECT_EQ(invalid.error->code, RendererErrorCode::INVALID_ARGUMENT);
    for (int mode = 0; mode < 3; ++mode) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        auto created = ScenePrepareScheduler::Create({2, 256, 1});
        auto frame   = store.Freeze();
        auto a       = DescribeRenderObjectPrepare(1, scene);
        auto b       = Reader(2, mode == 1 ? SceneId{0, 7} : scene);
        if (mode == 0) {
            a.after = {2};
            b.after = {1};
        }
        auto submission = created.scheduler->Submit(*frame, {a, b}, [](const auto &) { ADD_FAILURE(); });
        EXPECT_FALSE(submission.Accepted());
        ASSERT_TRUE(submission.error);
        EXPECT_EQ(submission.error->code, RendererErrorCode::INVALID_ARGUMENT);
        frame.reset();
        EXPECT_FALSE(store.IsFrozen());
    }
}

TEST(ParallelPrepare, CompletedGroupReleasesInternalBorrowsButRetainedInputViewStillPins)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto                            created = ScenePrepareScheduler::Create({1});
    auto                            frame   = store.Freeze();
    FrozenReadView<RenderTransform> held;
    SystemDesc                      read{1, "Input"};
    read.accesses = {ComponentAccess<RenderTransform>("scene", SceneAccessOwner(scene), AccessMode::READ)};
    auto submission =
        created.scheduler->Submit(*frame, {read}, [&](const FrozenPrepareContext &context) { held = context.Read<RenderTransform>(scene); });
    ASSERT_TRUE(submission.Accepted());
    EXPECT_TRUE(submission.group->Wait().Succeeded());
    frame.reset();
    EXPECT_TRUE(store.IsFrozen());
    created.scheduler.reset();
    EXPECT_EQ(submission.group->Poll(), CompletionState::SUCCEEDED);
    held = {};
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_TRUE(store.Apply(Initial(2)).Succeeded());
}

TEST(ParallelPrepare, EmptyPlansReuseAndFailuresHaveExplicitOutcomes)
{
    auto created = ScenePrepareScheduler::Create({1});
    ASSERT_TRUE(created.scheduler);
    for (uint32_t i = 0; i < 30; ++i) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        auto frame      = store.Freeze();
        auto submission = created.scheduler->Submit(*frame, {}, {});
        ASSERT_TRUE(submission.Accepted());
        EXPECT_TRUE(submission.group->Wait().Succeeded());
        frame.reset();
        EXPECT_FALSE(store.IsFrozen());
    }
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto frame      = store.Freeze();
    auto submission = created.scheduler->Submit(*frame, {SystemDesc{1, "Failure"}}, [](const auto &context) { context.Fail("explicit failure"); });
    ASSERT_TRUE(submission.Accepted());
    const auto &result = submission.group->Wait();
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
    EXPECT_FALSE(result.nodes[0].diagnostic.empty());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
