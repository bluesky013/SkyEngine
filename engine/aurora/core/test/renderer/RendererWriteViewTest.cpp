#include "frame/CpuRenderCoordinator.h"
#include "prepare/FrozenPrepareExecutor.h"
#include "prepare/RenderObjectPrepare.h"
#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <optional>
#include <thread>

using namespace sky;
using namespace sky::aurora;

namespace {
    constexpr SceneId         scene{0, 1};
    constexpr RenderEntityRef entity{scene, {2, 1}};

    FramePacket Packet(uint64_t sequence, std::vector<RenderCommand> commands = {})
    {
        FramePacket packet;
        packet.sequence = sequence;
        packet.commands = std::move(commands);
        return packet;
    }

    RenderEntityDesc Object(float x)
    {
        RenderEntityDesc object;
        object.world.v[12]              = x;
        object.mesh.resource.revision   = 7;
        object.material.bindingRevision = 9;
        object.visibilityMask           = 3;
        return object;
    }

    FramePacket Initial()
    {
        return Packet(1, {CreateScene{scene, {}}, CreateRenderEntity{entity, Object(4)}});
    }

    SystemDesc Access(SystemId id, AccessMode mode, SceneId owner = scene)
    {
        SystemDesc desc{id, "Objects"};
        desc.accesses = {ComponentAccess<RenderObjectData>("scene", SceneAccessOwner(owner), mode)};
        return desc;
    }

    DependencyPlan Compile(std::vector<SystemDesc> systems)
    {
        auto result = CompileDependencyPlan(systems);
        EXPECT_TRUE(std::holds_alternative<DependencyPlan>(result));
        return std::get<DependencyPlan>(std::move(result));
    }

    SerialPrepareResult
    ExecutePlan(CpuSceneStore &store, std::vector<SystemDesc> systems, const std::function<void(const FrozenPrepareContext &)> &kernel)
    {
        auto frame = store.Freeze();
        EXPECT_TRUE(frame);
        return FrozenPrepareExecutor::Run(*frame, Compile(std::move(systems)), kernel);
    }

    const RenderObjectData &Cached(const CpuSceneStore &store, RenderEntityRef id = entity)
    {
        const auto *state = store.FindScene(id.scene);
        return *state->registry.Get<RenderObjectData>(state->FindEntity(id.entity)->entity);
    }
} // namespace

TEST(FrozenWriteView, PermissionsRejectUndeclaredReadsWritesOwnersAndInactiveScopes)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto frame   = store.Freeze();
    auto pending = frame->Bind(Access(1, AccessMode::WRITE));
    ASSERT_TRUE(pending.context);
    EXPECT_FALSE(pending.context->Write<RenderObjectData>(scene));
    EXPECT_FALSE(pending.context->Read<RenderObjectData>(scene));
    auto result = FrozenPrepareExecutor::Run(*frame, Compile({Access(1, AccessMode::READ)}), [&](const FrozenPrepareContext &context) {
        EXPECT_FALSE(context.Write<RenderObjectData>(scene));
        EXPECT_FALSE(context.Read<RenderObjectData>((SceneId{0, 2})));
    });
    EXPECT_FALSE(result.Succeeded());
}

TEST(FrozenWriteView, UndeclaredJoinedInputAndResourceAliasesDoNotGrantComponentAccess)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto result = ExecutePlan(store, {Access(1, AccessMode::WRITE)},
                              [&](const FrozenPrepareContext &context) { EXPECT_FALSE((context.Write<RenderObjectData, RenderTransform>(scene))); });
    EXPECT_FALSE(result.Succeeded());
    SystemDesc resource{2, "ResourceOnly"};
    resource.accesses = {ResourceAccess("scene", SceneAccessOwner(scene), "RenderObjectData", AccessMode::WRITE)};
    result = ExecutePlan(store, {resource}, [&](const FrozenPrepareContext &context) { EXPECT_FALSE(context.Write<RenderObjectData>(scene)); });
    EXPECT_FALSE(result.Succeeded());
}

TEST(FrozenWriteView, SlicesUseStableEntityOrderAndCannotExpandParentRange)
{
    CpuSceneStore store;
    ASSERT_TRUE(store
                    .Apply(Packet(1, {CreateScene{scene, {}}, CreateRenderEntity{{scene, {9, 1}}, {}}, CreateRenderEntity{entity, {}},
                                      CreateRenderEntity{{scene, {4, 1}}, {}}}))
                    .Succeeded());
    auto result = ExecutePlan(store, {Access(1, AccessMode::WRITE), Access(2, AccessMode::READ)}, [&](const FrozenPrepareContext &context) {
        if (context.Id() == 1) {
            auto whole = context.Write<RenderObjectData>(scene);
            EXPECT_EQ(whole.Size(), 3u);
            auto middle = whole.Slice(1, 1);
            whole.Slice(3, 0).ForEach([](auto, auto &) { FAIL(); });
            middle.ForEach([](RenderEntityId id, RenderObjectData::Value &value) {
                EXPECT_EQ(id.index, 4u);
                value.visibilityMask = 17;
            });
        } else {
            auto                  all = context.Read<RenderObjectData>(scene);
            std::vector<uint32_t> ids;
            all.ForEach([&](RenderEntityId id, const RenderObjectData &data) {
                static_assert(std::is_const_v<std::remove_reference_t<decltype(data)>>);
                ids.push_back(id.index);
                EXPECT_EQ(data.valid, id.index == 4);
                if (data.valid) {
                    EXPECT_EQ(data.value.visibilityMask, 17u);
                    EXPECT_EQ(data.preparedSequence, 1u);
                    EXPECT_EQ(data.sourceRevision, 0u);
                }
            });
            EXPECT_EQ(ids, (std::vector<uint32_t>{2, 4, 9}));
        }
    });
    EXPECT_TRUE(result.Succeeded());
}

TEST(FrozenWriteView, SuccessPublishesBeforeSuccessorsAndCandidatesStartAfterPredecessors)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto first   = Access(20, AccessMode::WRITE);
    auto second  = Access(10, AccessMode::WRITE);
    second.after = {20};
    auto reader  = Access(1, AccessMode::READ);
    reader.after = {10};
    auto frame   = store.Freeze();
    auto plan    = Compile({reader, second, first});
    auto result  = FrozenPrepareExecutor::Run(*frame, plan, [&](const FrozenPrepareContext &context) {
        if (context.Id() == 20) {
            context.Write<RenderObjectData>(scene).ForEach([](auto, auto &value) { value.visibilityMask = 5; });
        } else if (context.Id() == 10) {
            context.Write<RenderObjectData>(scene).ForEach([](auto, auto &value) { value.visibilityMask += 2; });
        } else {
            context.Read<RenderObjectData>(scene).ForEach([](auto, const auto &data) {
                EXPECT_TRUE(data.valid);
                EXPECT_EQ(data.value.visibilityMask, 7u);
            });
        }
    });
    EXPECT_TRUE(result.Succeeded());
    EXPECT_FALSE(FrozenPrepareExecutor::Run(*frame, plan, [](const auto &) {}).Succeeded());
}

TEST(RenderObjectPrepare, RevisionZeroDirtyUpdatesAndUnchangedPacketsRetainSuccessfulStamp)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto prepare = [&] {
        return ExecutePlan(store, {DescribeRenderObjectPrepare(1, scene)},
                           [](const FrozenPrepareContext &context) { PrepareRenderObjects(context, scene); });
    };
    EXPECT_FALSE(Cached(store).valid);
    ASSERT_TRUE(prepare().Succeeded());
    EXPECT_TRUE(Cached(store).valid);
    EXPECT_EQ(Cached(store).sourceRevision, 0u);
    EXPECT_EQ(Cached(store).preparedSequence, 1u);
    EXPECT_EQ(Cached(store).value.world.v[12], 4);
    EXPECT_EQ(Cached(store).value.mesh.resource.revision, 7u);
    EXPECT_EQ(Cached(store).value.material.bindingRevision, 9u);
    EXPECT_EQ(Cached(store).value.visibilityMask, 3u);
    ASSERT_TRUE(store.Apply(Packet(2)).Succeeded());
    ASSERT_TRUE(prepare().Succeeded());
    EXPECT_EQ(Cached(store).preparedSequence, 1u);
    ASSERT_TRUE(store.Apply(Packet(3, {UpdateRenderEntity{entity, Object(8), 1}})).Succeeded());
    EXPECT_FALSE(Cached(store).valid);
    EXPECT_EQ(Cached(store).sourceRevision, 0u);
    ASSERT_TRUE(prepare().Succeeded());
    EXPECT_EQ(Cached(store).sourceRevision, 1u);
    EXPECT_EQ(Cached(store).preparedSequence, 3u);
    EXPECT_EQ(Cached(store).value.world.v[12], 8);
    auto stale = store.Apply(Packet(4, {UpdateRenderEntity{entity, Object(90), 1}}));
    ASSERT_TRUE(stale.Succeeded());
    EXPECT_EQ(stale.skipped.size(), 1u);
    ASSERT_TRUE(prepare().Succeeded());
    EXPECT_EQ(Cached(store).preparedSequence, 3u);
    EXPECT_EQ(Cached(store).value.world.v[12], 8);
}

TEST(RenderObjectPrepare, ReusedGenerationGetsFreshDerivedStorage)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    ASSERT_TRUE(ExecutePlan(store, {DescribeRenderObjectPrepare(1, scene)}, [](const FrozenPrepareContext &ctx) {
                    PrepareRenderObjects(ctx, scene);
                }).Succeeded());
    ASSERT_TRUE(store.Apply(Packet(2, {DestroyRenderEntity{entity}})).Succeeded());
    ASSERT_TRUE(store.AcknowledgeRetirement(entity));
    const RenderEntityRef next{scene, {entity.entity.index, 2}};
    ASSERT_TRUE(store.Apply(Packet(3, {CreateRenderEntity{next, Object(12)}})).Succeeded());
    EXPECT_FALSE(Cached(store, next).valid);
    EXPECT_EQ(Cached(store, next).preparedSequence, 0u);
    auto result = ExecutePlan(store, {DescribeRenderObjectPrepare(1, scene), Access(2, AccessMode::READ)}, [&](const FrozenPrepareContext &context) {
        if (context.Id() == 1) {
            PrepareRenderObjects(context, scene);
        } else {
            context.Read<RenderObjectData>(scene).ForEach([&](RenderEntityId id, const auto &value) {
                EXPECT_EQ(id, next.entity);
                EXPECT_EQ(value.value.world.v[12], 12);
                EXPECT_EQ(value.sourceRevision, 0u);
                EXPECT_EQ(value.preparedSequence, 3u);
            });
        }
    });
    EXPECT_TRUE(result.Succeeded());
}

TEST(FrozenWriteView, FailedKernelPublishesNoRowsOrStampsAndSkipsDependentReaders)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    ASSERT_TRUE(ExecutePlan(store, {DescribeRenderObjectPrepare(1, scene)}, [](const FrozenPrepareContext &ctx) {
                    PrepareRenderObjects(ctx, scene);
                }).Succeeded());
    ASSERT_TRUE(store.Apply(Packet(2, {UpdateRenderEntity{entity, Object(8), 1}})).Succeeded());
    FrozenWriteView<RenderObjectData> saved;
    bool                              independent = false;
    auto result = ExecutePlan(store, {Access(1, AccessMode::WRITE), Access(2, AccessMode::READ), SystemDesc{3, "Independent"}},
                              [&](const FrozenPrepareContext &context) {
                                  if (context.Id() == 1) {
                                      saved = context.Write<RenderObjectData>(scene);
                                      saved.ForEach([](auto, auto &value) { value.world.v[12] = 99; });
                                      context.Fail("injected failure after staging");
                                  } else if (context.Id() == 2) {
                                      FAIL() << "Failed writer must block its reader";
                                  } else {
                                      independent = true;
                                  }
                              });
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
    EXPECT_EQ(result.nodes[1].state, PrepareNodeState::SKIPPED);
    EXPECT_TRUE(independent);
    EXPECT_FALSE(saved.ForEach([](auto, auto &) {}));
    EXPECT_TRUE(store.IsFrozen());
    saved = {};
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_EQ(Cached(store).value.world.v[12], 4);
    EXPECT_EQ(Cached(store).preparedSequence, 1u);
    EXPECT_EQ(Cached(store).sourceRevision, 0u);
    EXPECT_FALSE(Cached(store).valid);
}

TEST(FrozenWriteView, RowFailureDiscardsAllStagedRows)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto result = ExecutePlan(store, {Access(1, AccessMode::WRITE)}, [&](const FrozenPrepareContext &context) {
        auto write = context.Write<RenderObjectData>(scene);
        EXPECT_TRUE(write.ForEach([](auto, auto &value) { value.visibilityMask = 5; }));
        EXPECT_FALSE(write.ForEach([&](auto, auto &value) {
            value.visibilityMask = 99;
            EXPECT_TRUE(context.Fail("row failed"));
        }));
    });
    EXPECT_FALSE(result.Succeeded());
    ASSERT_EQ(result.nodes.size(), 1u);
    EXPECT_EQ(result.nodes[0].diagnostic, "row failed");
    EXPECT_FALSE(Cached(store).valid);
    EXPECT_EQ(Cached(store).preparedSequence, 0u);
}

TEST(FrozenWriteView, InvalidSlicesFailTheSystemAndDiscardStaging)
{
    using Reject                       = std::function<void(const FrozenPrepareContext &)>;
    const std::array<Reject, 4> reject = {
        [](const auto &context) { EXPECT_FALSE(context.template Write<RenderObjectData>(scene).Slice(0, 0).Slice(0, 1)); },
        [](const auto &context) { EXPECT_FALSE(context.template Write<RenderObjectData>(scene).Slice(1, std::numeric_limits<size_t>::max())); },
        [](const auto &context) { EXPECT_FALSE(context.template Write<RenderObjectData>(scene).Slice(2, 0)); },
        [](const auto &context) { EXPECT_FALSE(context.template Read<RenderObjectData>(scene).Slice(1, 1)); }};
    for (const auto &invalidSlice : reject) {
        CpuSceneStore store;
        ASSERT_TRUE(store.Apply(Initial()).Succeeded());
        auto result = ExecutePlan(store, {Access(1, AccessMode::WRITE), Access(2, AccessMode::READ)}, [&](const auto &context) {
            context.template Write<RenderObjectData>(scene).ForEach([](auto, auto &value) { value.visibilityMask = 99; });
            invalidSlice(context);
        });
        EXPECT_FALSE(result.Succeeded());
        EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
        EXPECT_EQ(result.nodes[1].state, PrepareNodeState::SKIPPED);
        EXPECT_FALSE(Cached(store).valid);
    }
}

TEST(FrozenWriteView, CrossThreadAccessFailsProducerAndDrainsIndependentWork)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    bool independent = false;
    auto result      = ExecutePlan(store, {Access(1, AccessMode::WRITE), Access(2, AccessMode::READ), SystemDesc{3, "Independent"}},
                                   [&](const FrozenPrepareContext &context) {
                                  if (context.Id() == 1) {
                                      auto write = context.Write<RenderObjectData>(scene);
                                      write.ForEach([](auto, auto &value) { value.visibilityMask = 99; });
                                      std::thread other([&] { EXPECT_FALSE(write.ForEach([](auto, auto &) { ADD_FAILURE(); })); });
                                      other.join();
                                  } else if (context.Id() == 2) {
                                      ADD_FAILURE();
                                  } else {
                                      independent = true;
                                  }
                                   });
    EXPECT_FALSE(result.Succeeded());
    EXPECT_EQ(result.nodes[0].state, PrepareNodeState::FAILED);
    EXPECT_FALSE(result.nodes[0].diagnostic.empty());
    EXPECT_EQ(result.nodes[1].state, PrepareNodeState::SKIPPED);
    EXPECT_TRUE(independent);
    EXPECT_FALSE(Cached(store).valid);
}

TEST(FrozenWriteView, EscapedContextsAndDerivedViewsCannotCrossNodeBoundary)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    FrozenWriteView<RenderObjectData>   savedWrite;
    FrozenReadView<RenderObjectData>    savedRead;
    std::optional<FrozenPrepareContext> savedContext;
    auto result = ExecutePlan(store, {Access(1, AccessMode::WRITE), Access(2, AccessMode::READ), Access(3, AccessMode::WRITE)},
                              [&](const FrozenPrepareContext &context) {
                                  if (context.Id() == 1) {
                                      savedContext = context;
                                      savedWrite   = context.Write<RenderObjectData>(scene);
                                      savedWrite.ForEach([](auto, auto &value) { value.visibilityMask = 5; });
                                  } else if (context.Id() == 2) {
                                      EXPECT_FALSE(savedWrite.ForEach([](auto, auto &) {}));
                                      EXPECT_FALSE(savedContext->Write<RenderObjectData>(scene));
                                      savedRead = context.Read<RenderObjectData>(scene);
                                      savedRead.ForEach([](auto, const auto &data) { EXPECT_EQ(data.value.visibilityMask, 5u); });
                                  } else {
                                      EXPECT_FALSE(savedRead.ForEach([](auto, const auto &) {}));
                                      context.Write<RenderObjectData>(scene).ForEach([](auto, auto &value) { value.visibilityMask = 8; });
                                  }
                              });
    EXPECT_TRUE(result.Succeeded());
    EXPECT_FALSE(savedRead.ForEach([](auto, const auto &) {}));
    EXPECT_FALSE(savedContext->Read<RenderObjectData>(scene));
    savedContext.reset();
    savedRead  = {};
    savedWrite = {};
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_EQ(Cached(store).value.visibilityMask, 8u);
}

TEST(FrozenWriteView, InvalidFinalBindingPreventsAllWritersFromRunning)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(Initial()).Succeeded());
    auto invalid                             = Access(2, AccessMode::WRITE);
    invalid.accesses[0].key.owner.generation = 2;
    int calls                                = 0;
    EXPECT_FALSE(ExecutePlan(store, {Access(1, AccessMode::WRITE), invalid}, [&](const auto &) { ++calls; }).Succeeded());
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(store.IsFrozen());
    EXPECT_FALSE(Cached(store).valid);
}

TEST(RenderObjectPrepare, CoordinatorPreparesSharedSceneOnceAndViewsReadPublishedObjects)
{
    const SceneId otherScene{1, 1};
    auto          packet = std::make_unique<FramePacket>(Initial());
    packet->commands.emplace_back(CreateScene{otherScene, {}});
    packet->commands.emplace_back(CreateRenderEntity{{otherScene, {2, 1}}, Object(16)});
    packet->commands.emplace_back(CreateViewport{{0, 1}, {}});
    for (uint32_t index = 0; index < 3; ++index) {
        ViewDesc view;
        view.scene  = index < 2 ? scene : otherScene;
        view.output = {0, 1};
        packet->commands.emplace_back(CreateView{{index, 1}, view});
    }
    std::atomic_uint32_t prepares = 0;
    std::atomic_uint32_t views    = 0;
    CpuRenderCoordinator coordinator;
    ASSERT_FALSE(coordinator.Init({1, 1}, [&](const FrozenCpuFrame &frame) {
        CpuPrepareProgram program;
        for (auto id : frame.Scenes()) {
            program.systems.push_back(DescribeRenderObjectPrepare(id.index + 1, id));
        }
        for (const auto &view : frame.Views()) {
            auto desc  = Access(100 + view.view.id.index, AccessMode::READ, view.view.desc.scene);
            desc.after = {view.view.desc.scene.index + 1};
            program.systems.push_back(std::move(desc));
        }
        program.kernel = [&](const FrozenPrepareContext &context) {
            if (context.Id() < 100) {
                ++prepares;
                PrepareRenderObjects(context, {static_cast<uint32_t>(context.Id() - 1), 1});
            } else {
                ++views;
                auto id = context.Views()[context.Id() - 100].view.desc.scene;
                context.Read<RenderObjectData>(id).ForEach([&](auto, const auto &data) {
                    EXPECT_TRUE(data.valid);
                    EXPECT_EQ(data.value.world.v[12], id == scene ? 4 : 16);
                });
            }
        };
        return program;
    }));
    ASSERT_EQ(coordinator.TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
    coordinator.RequestShutdown();
    coordinator.Join();
    std::array<CpuCoordinatorEvent, 2> events;
    ASSERT_EQ(coordinator.PollEvents(events), 2u);
    EXPECT_TRUE(std::get<CpuPacketCompleted>(events[0]).Succeeded());
    EXPECT_FALSE(std::get<CpuCoordinatorStopped>(events[1]).error);
    EXPECT_EQ(prepares.load(), 2u);
    EXPECT_EQ(views.load(), 3u);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
