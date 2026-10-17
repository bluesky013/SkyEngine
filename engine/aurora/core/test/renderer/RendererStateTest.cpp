#include "frame/FramePacketQueue.h"
#include "prepare/SerialPrepareExecutor.h"
#include "scene/CpuSceneStore.h"
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <future>
#include <limits>
#include <thread>

using namespace sky;
using namespace sky::aurora;

namespace {
    constexpr SceneId         scene{0, 1};
    constexpr ViewId          viewA{0, 1};
    constexpr ViewId          viewB{1, 1};
    constexpr ViewportId      outputA{0, 1};
    constexpr ViewportId      outputB{1, 1};
    constexpr RenderEntityRef entity{scene, {0, 1}};

    FramePacket Packet(uint64_t sequence, std::vector<RenderCommand> commands = {})
    {
        FramePacket packet;
        packet.sequence = sequence;
        packet.commands = std::move(commands);
        return packet;
    }

    std::unique_ptr<FramePacket> OwnedPacket(uint64_t sequence, std::vector<RenderCommand> commands = {})
    {
        return std::make_unique<FramePacket>(Packet(sequence, std::move(commands)));
    }

    FramePacket InitialPacket()
    {
        ViewDesc a;
        a.scene            = scene;
        a.output           = outputA;
        a.visibilityMask   = 1;
        ViewDesc b         = a;
        b.output           = outputB;
        b.cameraPosition.x = 10;
        b.visibilityMask   = 2;
        ViewportDesc second;
        second.extent = {800, 600};
        RenderEntityDesc desc;
        desc.visibilityMask = 1;
        return Packet(1, {CreateScene{scene, {}}, CreateRenderEntity{entity, desc}, CreateViewport{outputA, {}}, CreateViewport{outputB, second},
                          CreateView{viewA, a}, CreateView{viewB, b}});
    }

    void ExpectSkipped(const PacketApplyResult &result, size_t index, CommandSkipReason reason)
    {
        ASSERT_TRUE(result.Succeeded());
        ASSERT_EQ(result.skipped.size(), 1u);
        EXPECT_EQ(result.skipped[0].commandIndex, index);
        EXPECT_EQ(result.skipped[0].reason, reason);
    }
} // namespace

TEST(FramePacketQueue, ExactCapacityOwnershipSequenceAndCloseDrain)
{
    FramePacketQueue queue;
    auto             early = OwnedPacket(1);
    EXPECT_EQ(queue.TrySubmit(early).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(early);
    EXPECT_FALSE(queue.WaitPop());
    EXPECT_FALSE(queue.Init(0));
    ASSERT_TRUE(queue.Init(1));
    EXPECT_FALSE(queue.Init(2));
    auto first = OwnedPacket(1, {CreateScene{scene, {}}});
    EXPECT_EQ(queue.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    EXPECT_FALSE(first);
    auto second = OwnedPacket(2);
    second->extensions.push_back({{}, {1, 2, 3}});
    const auto *original = second.get();
    const auto *bytes    = second->extensions[0].bytes.data();
    EXPECT_EQ(queue.TrySubmit(second).status, PacketSubmitStatus::BUSY);
    EXPECT_EQ(second.get(), original);
    EXPECT_EQ(second->extensions[0].bytes.data(), bytes);
    auto duplicate = OwnedPacket(1);
    EXPECT_EQ(queue.TrySubmit(duplicate).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(duplicate);
    auto popped = queue.TryPop();
    ASSERT_TRUE(popped);
    EXPECT_EQ(popped->sequence, 1u);
    EXPECT_TRUE(std::holds_alternative<CreateScene>(popped->commands[0]));
    EXPECT_EQ(queue.TrySubmit(second).status, PacketSubmitStatus::ACCEPTED);
    queue.Close();
    queue.Close();
    auto third = OwnedPacket(3);
    EXPECT_EQ(queue.TrySubmit(third).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(third);
    popped = queue.WaitPop();
    ASSERT_TRUE(popped);
    EXPECT_EQ(popped.get(), original);
    EXPECT_EQ(popped->extensions[0].bytes, (std::vector<uint8_t>{1, 2, 3}));
    EXPECT_FALSE(queue.WaitPop());
    EXPECT_FALSE(queue.TryPop());
}

TEST(FramePacketQueue, InvalidPacketsAndExhaustedSequenceCannotWrap)
{
    FramePacketQueue queue;
    ASSERT_TRUE(queue.Init(2));
    std::unique_ptr<FramePacket> empty;
    EXPECT_EQ(queue.TrySubmit(empty).status, PacketSubmitStatus::REJECTED);
    auto zero = OwnedPacket(0);
    EXPECT_EQ(queue.TrySubmit(zero).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(zero);
    auto last = OwnedPacket(std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(queue.TrySubmit(last).status, PacketSubmitStatus::ACCEPTED);
    auto wrapped = OwnedPacket(1);
    EXPECT_EQ(queue.TrySubmit(wrapped).status, PacketSubmitStatus::REJECTED);
    EXPECT_TRUE(wrapped);
}

TEST(FramePacketQueue, CloseWakesIdleConsumer)
{
    FramePacketQueue queue;
    ASSERT_TRUE(queue.Init(2));
    std::promise<void> started;
    std::promise<bool> finished;
    auto               ready  = started.get_future();
    auto               result = finished.get_future();
    std::thread        consumer([&] {
        started.set_value();
        finished.set_value(queue.WaitPop() == nullptr);
    });
    ready.wait();
    queue.Close();
    EXPECT_EQ(result.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    consumer.join();
    EXPECT_TRUE(result.get());
}

TEST(FramePacketQueue, ConcurrentProducerConsumerPreserveEveryLifecyclePacket)
{
    FramePacketQueue queue;
    ASSERT_TRUE(queue.Init(3));
    std::vector<uint64_t> received;
    std::thread           consumer([&] {
        while (auto packet = queue.WaitPop()) {
            received.push_back(packet->sequence);
        }
    });
    const auto            deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    uint64_t              accepted = 0;
    for (uint64_t sequence = 1; sequence <= 2000; ++sequence) {
        auto               packet = OwnedPacket(sequence, {DestroyScene{scene}});
        PacketSubmitStatus status;
        do {
            status = queue.TrySubmit(packet).status;
            if (status == PacketSubmitStatus::BUSY) {
                std::this_thread::yield();
            }
        } while (status == PacketSubmitStatus::BUSY && std::chrono::steady_clock::now() < deadline);
        if (status != PacketSubmitStatus::ACCEPTED) {
            ADD_FAILURE() << "Submission did not progress at " << sequence;
            break;
        }
        ++accepted;
    }
    queue.Close();
    consumer.join();
    ASSERT_EQ(received.size(), accepted);
    ASSERT_EQ(accepted, 2000u);
    for (size_t i = 0; i < received.size(); ++i) {
        EXPECT_EQ(received[i], i + 1);
    }
}

TEST(CpuSceneStore, PersistentEcsAndIndependentViewsSelectSceneOnce)
{
    CpuSceneStore store;
    auto          packet = InitialPacket();
    const auto    result = store.Apply(packet);
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.changedScenes, (std::vector<SceneId>{scene}));
    packet.commands.clear();
    const auto *state = store.FindScene(scene);
    ASSERT_NE(state, nullptr);
    const auto *record = state->FindEntity(entity.entity);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(state->registry.Get<RenderVisibility>(record->entity)->mask, 1u);
    EXPECT_EQ(state->registry.Get<RenderTransform>(record->entity)->world.v[0], 1);
    const auto internalId = record->entity;
    const auto selection  = store.SelectPrepareInputs();
    EXPECT_EQ(selection.scenes, (std::vector<SceneId>{scene}));
    EXPECT_EQ(selection.views, (std::vector<ViewId>{viewA, viewB}));
    EXPECT_EQ(store.FindView(viewB)->desc.cameraPosition.x, 10);
    EXPECT_EQ(store.FindViewport(outputB)->desc.extent.width, 800u);
    EXPECT_TRUE(store.Apply(Packet(2)).Succeeded());
    EXPECT_EQ(store.FindScene(scene), state);
    EXPECT_EQ(store.FindScene(scene)->FindEntity(entity.entity)->entity, internalId);
    EXPECT_EQ(store.FindScene(scene)->lastChangedSequence, 1u);
}

TEST(CpuSceneStore, RevisionsAndCommandOrderCannotResurrectDeletedEntities)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(InitialPacket()).Succeeded());
    RenderEntityDesc updated;
    updated.visibilityMask = 8;
    updated.world.v[12]    = 42;
    auto stale             = updated;
    stale.visibilityMask   = 4;
    auto result            = store.Apply(Packet(2, {UpdateRenderEntity{entity, updated, 4}, UpdateRenderEntity{entity, stale, 3}}));
    ExpectSkipped(result, 1, CommandSkipReason::STALE_REVISION);
    const auto *state  = store.FindScene(scene);
    const auto *record = state->FindEntity(entity.entity);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->revision, 4u);
    EXPECT_EQ(state->registry.Get<RenderVisibility>(record->entity)->mask, 8u);
    EXPECT_EQ(state->registry.Get<RenderTransform>(record->entity)->world.v[12], 42);
    const auto internal = record->entity;
    result              = store.Apply(Packet(3, {DestroyRenderEntity{entity}, UpdateRenderEntity{entity, stale, 5}}));
    ExpectSkipped(result, 1, CommandSkipReason::STALE_HANDLE);
    EXPECT_EQ(result.retirements.size(), 1u);
    EXPECT_EQ(result.changedScenes.size(), 1u);
    EXPECT_FALSE(state->registry.IsAlive(internal));
    EXPECT_EQ(state->registry.Get<RenderTransform>(internal), nullptr);
    EXPECT_EQ(state->FindEntity(entity.entity), nullptr);
}

TEST(CpuSceneStore, EntityReuseWaitsForAcknowledgementAndRejectsOldGeneration)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(InitialPacket()).Succeeded());
    auto removed = store.Apply(Packet(2, {DestroyRenderEntity{entity}}));
    ASSERT_EQ(removed.retirements.size(), 1u);
    RenderEntityRef replacement{scene, {entity.entity.index, 2}};
    ExpectSkipped(store.Apply(Packet(3, {CreateRenderEntity{replacement, {}}})), 0, CommandSkipReason::RETIREMENT_PENDING);
    EXPECT_FALSE(store.AcknowledgeRetirement(replacement));
    EXPECT_TRUE(store.AcknowledgeRetirement(removed.retirements[0]));
    EXPECT_FALSE(store.AcknowledgeRetirement(removed.retirements[0]));
    ASSERT_TRUE(store.Apply(Packet(4, {CreateRenderEntity{replacement, {}}})).Succeeded());
    auto late = store.Apply(Packet(5, {DestroyRenderEntity{entity}, CreateRenderEntity{entity, {}}, UpdateRenderEntity{entity, {}, 99}}));
    EXPECT_EQ(late.skipped.size(), 3u);
    EXPECT_TRUE(late.retirements.empty());
    ASSERT_NE(store.FindScene(scene)->FindEntity(replacement.entity), nullptr);
    EXPECT_EQ(store.FindScene(scene)->entities.size(), 1u);
}

TEST(CpuSceneStore, SceneDestructionPreservesViewsAndOutputsUntilExplicitRebinding)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(InitialPacket()).Succeeded());
    auto removed = store.Apply(Packet(2, {DestroyScene{scene}}));
    ASSERT_EQ(removed.retirements.size(), 1u);
    EXPECT_EQ(store.FindScene(scene), nullptr);
    EXPECT_NE(store.FindView(viewA), nullptr);
    EXPECT_NE(store.FindViewport(outputA), nullptr);
    EXPECT_TRUE(store.SelectPrepareInputs().views.empty());
    SceneId replacement{scene.index, 2};
    ExpectSkipped(store.Apply(Packet(3, {CreateScene{replacement, {}}})), 0, CommandSkipReason::RETIREMENT_PENDING);
    ASSERT_TRUE(store.AcknowledgeRetirement(removed.retirements[0]));
    ASSERT_TRUE(store.Apply(Packet(4, {CreateScene{replacement, {}}})).Succeeded());
    EXPECT_TRUE(store.SelectPrepareInputs().views.empty());
    ViewDesc rebound = store.FindView(viewA)->desc;
    rebound.scene    = replacement;
    auto result =
        store.Apply(Packet(5, {UpdateRenderEntity{entity, {}, 1}, UpdateView{viewB, store.FindView(viewB)->desc, 1}, UpdateView{viewA, rebound, 1}}));
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.skipped.size(), 2u);
    EXPECT_EQ(store.SelectPrepareInputs().views, (std::vector<ViewId>{viewA}));
    EXPECT_EQ(store.SelectPrepareInputs().scenes, (std::vector<SceneId>{replacement}));
}

TEST(CpuSceneStore, ViewAndOutputRetirementAreIndependentAndGenerationChecked)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(InitialPacket()).Succeeded());
    auto resized = store.Apply(Packet(2, {ResizeViewport{outputA, {0, 0}, 3}, ResizeViewport{outputA, {9, 9}, 2}}));
    ExpectSkipped(resized, 1, CommandSkipReason::STALE_REVISION);
    EXPECT_EQ(store.SelectPrepareInputs().views, (std::vector<ViewId>{viewB}));
    ASSERT_TRUE(store.Apply(Packet(3, {ResizeViewport{outputA, {640, 480}, 4}})).Succeeded());
    EXPECT_EQ(store.SelectPrepareInputs().views.size(), 2u);
    auto removed = store.Apply(Packet(4, {DestroyViewport{outputA}, DestroyView{viewB}}));
    ASSERT_EQ(removed.retirements.size(), 2u);
    EXPECT_NE(store.FindView(viewA), nullptr);
    EXPECT_NE(store.FindViewport(outputB), nullptr);
    EXPECT_NE(store.FindScene(scene), nullptr);
    EXPECT_TRUE(store.SelectPrepareInputs().views.empty());
    const ViewportId nextOutput{outputA.index, 2};
    const ViewId     nextView{viewB.index, 2};
    ViewDesc         desc = store.FindView(viewA)->desc;
    desc.output           = outputB;
    auto pending          = store.Apply(Packet(5, {CreateViewport{nextOutput, {}}, CreateView{nextView, desc}}));
    ASSERT_EQ(pending.skipped.size(), 2u);
    for (const auto &retirement : removed.retirements) {
        ASSERT_TRUE(store.AcknowledgeRetirement(retirement));
    }
    ASSERT_TRUE(store.Apply(Packet(6, {CreateViewport{nextOutput, {}}, CreateView{nextView, desc}})).Succeeded());
    EXPECT_EQ(store.SelectPrepareInputs().views, (std::vector<ViewId>{nextView}));
    EXPECT_EQ(store.FindViewport(outputA), nullptr);
    EXPECT_EQ(store.FindView(viewB), nullptr);
}

TEST(CpuSceneStore, PreflightRejectsEntireMalformedPacketWithoutApplyingEarlierCommands)
{
    CpuSceneStore store;
    ASSERT_TRUE(store.Apply(InitialPacket()).Succeeded());
    RenderEntityDesc invalid;
    invalid.world.v[0] = std::numeric_limits<float>::quiet_NaN();
    const auto result  = store.Apply(Packet(2, {DestroyView{viewA}, UpdateRenderEntity{entity, invalid, 1}}));
    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.error->code, RendererErrorCode::INVALID_PACKET);
    EXPECT_TRUE(result.retirements.empty());
    EXPECT_NE(store.FindView(viewA), nullptr);
    EXPECT_FALSE(store.IsFailed());
    EXPECT_FALSE(store.Apply(Packet(2)).Succeeded());
    auto update             = store.FindView(viewA)->desc;
    update.cameraPosition.x = 5;
    ASSERT_TRUE(store.Apply(Packet(3, {UpdateView{viewA, update, 1}})).Succeeded());
    update.cameraPosition.x = 90;
    ExpectSkipped(store.Apply(Packet(4, {UpdateView{viewA, update, 1}})), 0, CommandSkipReason::STALE_REVISION);
    EXPECT_EQ(store.FindView(viewA)->desc.cameraPosition.x, 5);
}

TEST(CpuSceneStore, UnsupportedPayloadsAreNotSilentlyConsumed)
{
    CpuSceneStore store;
    auto          packet = Packet(1, {CreateScene{scene, {}}, ResourceCpuReady{}});
    auto         &bytes  = std::get<ResourceCpuReady>(packet.commands[1]).payload.bytes;
    bytes                = {9, 8, 7};
    auto result          = store.Apply(packet);
    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.error->code, RendererErrorCode::UNSUPPORTED);
    EXPECT_EQ(store.FindScene(scene), nullptr);
    EXPECT_EQ(bytes, (std::vector<uint8_t>{9, 8, 7}));
    packet = Packet(2, {CreateScene{scene, {}}});
    packet.extensions.push_back({{}, {1}});
    EXPECT_FALSE(store.Apply(packet).Succeeded());
    EXPECT_EQ(store.FindScene(scene), nullptr);
    EXPECT_TRUE(store.Apply(Packet(3, {CreateScene{scene, {}}})).Succeeded());
}

TEST(CpuSceneStore, SparseHostIndicesAndMaximumGenerationNeverReviveOldHandles)
{
    CpuSceneStore store;
    const SceneId last{std::numeric_limits<uint32_t>::max() - 1, std::numeric_limits<uint32_t>::max()};
    ASSERT_TRUE(store.Apply(Packet(1, {CreateScene{last, {}}, CreateRenderEntity{{last, {77, 1}}, {}}})).Succeeded());
    ASSERT_NE(store.FindScene(last), nullptr);
    auto removed = store.Apply(Packet(2, {DestroyScene{last}}));
    ASSERT_EQ(removed.retirements.size(), 1u);
    ASSERT_TRUE(store.AcknowledgeRetirement(removed.retirements[0]));
    ExpectSkipped(store.Apply(Packet(3, {CreateScene{{last.index, 1}, {}}})), 0, CommandSkipReason::STALE_HANDLE);
    EXPECT_EQ(store.FindScene(last), nullptr);
    EXPECT_FALSE(store.Apply(Packet(4, {CreateScene{{last.index, 0}, {}}})).Succeeded());
    EXPECT_FALSE(store.Apply(Packet(5, {CreateScene{{std::numeric_limits<uint32_t>::max(), 1}, {}}})).Succeeded());
}

TEST(CpuSceneStore, QueueToStoreToSerialPrepareUsesSharedSceneAndSeparateViewResults)
{
    FramePacketQueue queue;
    ASSERT_TRUE(queue.Init(2));
    auto first  = std::make_unique<FramePacket>(InitialPacket());
    auto second = OwnedPacket(2, {DestroyRenderEntity{entity}});
    ASSERT_EQ(queue.TrySubmit(first).status, PacketSubmitStatus::ACCEPTED);
    ASSERT_EQ(queue.TrySubmit(second).status, PacketSubmitStatus::ACCEPTED);
    queue.Close();
    std::array<std::vector<RenderEntityId>, 2> visible;
    uint32_t                                   sceneRuns = 0;
    CpuSceneStore                              store;
    for (uint64_t frame = 1; frame <= 2; ++frame) {
        auto packet = queue.WaitPop();
        ASSERT_TRUE(packet);
        ASSERT_EQ(packet->sequence, frame);
        ASSERT_TRUE(store.Apply(*packet).Succeeded());
        const auto selection = store.SelectPrepareInputs();
        ASSERT_EQ(selection.scenes.size(), 1u);
        ASSERT_EQ(selection.views.size(), 2u);
        SystemDesc prepare{1, "SharedScene"};
        prepare.accesses = {ResourceAccess("scene", {1, 1}, "ready", AccessMode::WRITE)};
        std::vector<SystemDesc> systems{prepare};
        for (size_t i = 0; i < selection.views.size(); ++i) {
            SystemDesc view{2 + i, "View"};
            view.after    = {1};
            view.accesses = {ResourceAccess("scene", {1, 1}, "ready", AccessMode::READ)};
            systems.push_back(std::move(view));
        }
        auto compiled = CompileDependencyPlan(systems);
        ASSERT_TRUE(std::holds_alternative<DependencyPlan>(compiled));
        const auto result = ExecuteSerialPrepare(std::get<DependencyPlan>(compiled), [&](SystemId id) {
            if (id == 1) {
                ++sceneRuns;
                return;
            }
            auto &output = visible[id - 2];
            output.clear();
            const auto &view  = *store.FindView(selection.views[id - 2]);
            const auto &state = *store.FindScene(view.desc.scene);
            for (const auto &[index, record] : state.entities) {
                if (state.registry.Get<RenderVisibility>(record.entity)->mask & view.desc.visibilityMask) {
                    output.push_back(record.id);
                }
            }
        });
        EXPECT_TRUE(result.Succeeded());
        EXPECT_EQ(sceneRuns, frame);
        EXPECT_EQ(visible[0].size(), frame == 1 ? 1u : 0u);
        EXPECT_TRUE(visible[1].empty());
    }
    EXPECT_FALSE(queue.WaitPop());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
