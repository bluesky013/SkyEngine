#include <aurora/adaptor/AuroraReflection.h>
#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/components/CameraComponent.h>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/EnvironmentComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <aurora/pipeline/SceneRenderFeature.h>
#include <aurora/renderer/IRenderer.h>
#include <framework/asset/AssetEvent.h>
#include <framework/world/TransformComponent.h>
#include <framework/world/World.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;
namespace {
    constexpr SceneId SCENE{0, 1};
    struct Source : ISceneResourceSource {
        bool             pending      = false;
        bool             staticOpaque = false;
        SceneImageResult ResolveImage(const Uuid &id, RendererInfo info) override
        {
            if (pending)
                return {};
            auto payload            = std::make_shared<RenderResourcePayload>();
            payload->resource       = {id, 1, 2, info.deviceEpoch};
            payload->format         = Name("linear-environment");
            payload->metadata.image = RenderImageLayout{2, 1};
            payload->bytes.resize(8, 128);
            return std::shared_ptr<const RenderResourcePayload>(std::move(payload));
        }
        SceneResourceResult ResolveMesh(const Uuid &id, const Uuid &, RendererInfo info) override
        {
            if (pending)
                return {};
            auto              data = std::make_shared<SceneMeshResources>();
            RenderResourceKey mesh{id, 1, 1, info.deviceEpoch}, material{Uuid::CreateWithSeed(901), 1, 1, info.deviceEpoch};
            data->parts.push_back({0, {mesh, 1}, {material, 1}});
            if (staticOpaque)
                data->parts.back().surface = SceneSurfaceFacts::StaticOpaque();
            data->payloads.push_back({mesh, Name("mesh"), {1, 2, 3, 4}});
            data->payloads.push_back({material, Name("material"), {5, 6, 7, 8}});
            if (staticOpaque)
                data->payloads.front().mesh =
                    RenderMeshData{{{0, 36, 12}}, {{0, 0, 0, Format::F_RGB32, "POSITION", 0}}, {{36, 6, IndexType::U16, {3}}}};
            return std::shared_ptr<const SceneMeshResources>(data);
        }
    };
    template <typename T>
    std::vector<T> Commands(const FramePacket &packet)
    {
        std::vector<T> result;
        for (const auto &command : packet.commands)
            if (const auto *value = std::get_if<T>(&command))
                result.push_back(*value);
        return result;
    }
    class SceneBridgeTest : public testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            auto *context = SerializationContext::Get();
        }
        WorldPtr                world  = World::CreateWorld();
        std::shared_ptr<Source> source = std::make_shared<Source>();
        RenderSceneBridge       bridge{{API::VULKAN, 7}, source};
        uint64_t                sequence = 0;
        FramePacket             Sync()
        {
            FramePacket packet;
            packet.sequence = ++sequence;
            EXPECT_FALSE(bridge.Append(packet, SCENE));
            return packet;
        }
        Actor *MeshActor()
        {
            auto *actor = world->CreateActor();
            actor->AddComponent<StaticMeshComponent>()->SetMeshUuid(Uuid::CreateWithSeed(900));
            return actor;
        }
    };
} // namespace
TEST_F(SceneBridgeTest, ParentTransformAndMaterialRevisionReachOwnedPacket)
{
    auto *parent = world->CreateActor();
    auto *actor  = MeshActor();
    actor->GetComponent<TransformComponent>()->SetParentPreserveLocal(parent->GetComponent<TransformComponent>());
    bridge.SetWorld(world.Get());
    const auto first = Sync();
    ASSERT_EQ(Commands<CreateRenderEntity>(first).size(), 1u);
    parent->GetComponent<TransformComponent>()->SetLocalTranslation({2, 3, 4});
    const auto changed = Sync();
    const auto updates = Commands<UpdateRenderEntity>(changed);
    ASSERT_EQ(updates.size(), 1u);
    EXPECT_EQ(updates[0].desc.world.v[12], 2);
    EXPECT_EQ(updates[0].desc.world.v[13], 3);
    EXPECT_EQ(updates[0].desc.world.v[14], 4);
    EXPECT_EQ(updates[0].revision, 1u);
    EXPECT_EQ(updates[0].desc.mesh.bindingRevision, 2u);
    EXPECT_TRUE(Sync().commands.empty());
    bridge.SetWorld(nullptr);
    world.Reset(nullptr);
    source.reset();
    EXPECT_EQ(Commands<ResourceCpuReady>(first).front().payload.bytes, (std::vector<uint8_t>{1, 2, 3, 4}));
}
TEST_F(SceneBridgeTest, RemovedComponentWaitsForExactRetirementBeforeGenerationReuse)
{
    auto *actor = MeshActor();
    bridge.SetWorld(world.Get());
    const auto original = Commands<CreateRenderEntity>(Sync()).front().id;
    actor->RemoveComponent<StaticMeshComponent>();
    const auto removed = Sync();
    ASSERT_EQ(Commands<DestroyRenderEntity>(removed).size(), 1u);
    EXPECT_EQ(removed.releasedResources.size(), 2u);
    actor->AddComponent<StaticMeshComponent>()->SetMeshUuid(Uuid::CreateWithSeed(900));
    const auto replacement = Commands<CreateRenderEntity>(Sync()).front().id;
    EXPECT_NE(original.entity.index, replacement.entity.index);
    bridge.OnEvent(EntityDestroyed{{SCENE, {original.entity.index, original.entity.generation + 1}}});
    bridge.OnEvent(EntityDestroyed{original});
    actor->RemoveComponent<StaticMeshComponent>();
    actor->AddComponent<StaticMeshComponent>()->SetMeshUuid(Uuid::CreateWithSeed(900));
    const auto reused = Commands<CreateRenderEntity>(Sync()).front().id;
    EXPECT_EQ(reused.entity.index, original.entity.index);
    EXPECT_EQ(reused.entity.generation, original.entity.generation + 1);
    bridge.OnEvent(EntityDestroyed{original});
    EXPECT_TRUE(Sync().commands.empty());
}
TEST_F(SceneBridgeTest, PendingAssetsAndWorldSwitchDoNotResurrectOldObjects)
{
    MeshActor();
    source->pending = true;
    bridge.SetWorld(world.Get());
    EXPECT_TRUE(Sync().commands.empty());
    source->pending     = false;
    const auto original = Commands<CreateRenderEntity>(Sync()).front().id;
    WorldPtr   other    = World::CreateWorld();
    bridge.SetWorld(other.Get());
    const auto removed = Commands<DestroyRenderEntity>(Sync());
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0].id, original);
    bridge.OnEvent(EntityDestroyed{original});
    EXPECT_TRUE(Sync().commands.empty());
}
TEST_F(SceneBridgeTest, CameraAndDirectionalLightUseWorldTransform)
{
    auto *actor = world->CreateActor();
    actor->AddComponent<CameraComponent>();
    actor->AddComponent<DirectLightComponent>();
    actor->GetComponent<TransformComponent>()->SetLocalTranslation({0, 0, 5});
    bridge.SetWorld(world.Get());
    const auto light = Commands<CreateRenderEntity>(Sync());
    ASSERT_EQ(light.size(), 1u);
    EXPECT_EQ(light[0].desc.properties.light.kind, RenderLightKind::DIRECTIONAL);
    const auto camera = bridge.Camera(SCENE, {0, 1}, {640, 360});
    ASSERT_TRUE(camera);
    EXPECT_EQ(camera->view.v[14], -5);
    EXPECT_EQ(camera->projection.v[15], 0);
    actor->GetComponent<TransformComponent>()->SetLocalScale({0, 0, 0});
    EXPECT_FALSE(bridge.Camera(SCENE, {0, 1}, {640, 360}));
}
TEST_F(SceneBridgeTest, EnvironmentPendingReplacementAndDisableKeepOwnedVersions)
{
    auto      *component = world->CreateActor()->AddComponent<EnvironmentComponent>();
    const auto first = Uuid::CreateWithSeed(601), second = Uuid::CreateWithSeed(602);
    component->SetDiffuseIrradiance(first);
    bridge.SetWorld(world.Get());
    const auto sync = [&] {
        FramePacket packet;
        packet.sequence = ++sequence;
        EXPECT_FALSE(bridge.Append(packet, SCENE, {0, 1}));
        return packet;
    };
    source->pending = true;
    auto pending    = sync();
    ASSERT_EQ(pending.extensions.size(), 1u);
    EXPECT_TRUE(pending.extensions[0].resources.empty());
    source->pending = false;
    auto ready      = sync();
    ASSERT_EQ(Commands<ResourceCpuReady>(ready).size(), 1u);
    ASSERT_EQ(ready.extensions[0].resources.size(), 1u);
    EXPECT_EQ(ready.extensions[0].resources[0].product, first);
    EXPECT_FALSE(CreateSceneRenderFeature(Name("aurora.scene"))->Validate(ready.extensions[0]));
    EXPECT_TRUE(Commands<ResourceCpuReady>(sync()).empty());
    component->SetDiffuseIrradiance(second);
    auto replacement = sync();
    EXPECT_EQ(Commands<ResourceCpuReady>(replacement).size(), 1u);
    ASSERT_EQ(replacement.releasedResources.size(), 1u);
    EXPECT_EQ(replacement.releasedResources[0].product, first);
    EXPECT_EQ(ready.extensions[0].resources[0].product, first);
    component->SetEnabled(false);
    auto disabled = sync();
    EXPECT_TRUE(disabled.extensions[0].resources.empty());
    ASSERT_EQ(disabled.releasedResources.size(), 1u);
    EXPECT_EQ(disabled.releasedResources[0].product, second);
    world->CreateActor()->AddComponent<EnvironmentComponent>();
    EXPECT_TRUE(sync().extensions.empty());
    EXPECT_FALSE(bridge.GetIssues().empty());
}

TEST(SceneMeshConversion, SharedSectionsValidateIndicesAndOwnedBacking)
{
    Asset<Mesh> asset;
    asset.SetUuid(ScenePrimitiveId(ScenePrimitive::CUBE));
    asset.Data()           = ScenePrimitiveMesh(ScenePrimitive::CUBE);
    asset.Data().subMeshes = {{0, 18, 0}, {18, 18, 1}};
    const auto result      = ConvertSceneMesh(asset, {API::VULKAN, 5});
    ASSERT_TRUE(std::holds_alternative<RenderResourcePayload>(result));
    const auto &payload = std::get<RenderResourcePayload>(result);
    ASSERT_TRUE(payload.mesh);
    ASSERT_EQ(payload.mesh->lods.size(), 1u);
    ASSERT_EQ(payload.mesh->lods[0].sections.size(), 2u);
    EXPECT_EQ(payload.mesh->lods[0].sections[1].firstIndex, 18u);
    asset.Data().indexData[0] = 255;
    EXPECT_TRUE(std::holds_alternative<RendererError>(ConvertSceneMesh(asset, {API::VULKAN, 5})));
    EXPECT_NE(payload.bytes[payload.mesh->lods[0].offset], 255);
}

TEST_F(SceneBridgeTest, BusyAndRejectedKeepTheSameOwnedSnapshot)
{
    struct Recorder final : IRenderer {
        PacketSubmitStatus           next = PacketSubmitStatus::BUSY;
        std::unique_ptr<FramePacket> accepted;
        PacketSubmitResult           TrySubmit(std::unique_ptr<FramePacket> &packet) override
        {
            if (next == PacketSubmitStatus::ACCEPTED)
                accepted = std::move(packet);
            return {next, {RendererErrorCode::NOT_READY, "recording fixture"}};
        }
        size_t PollEvents(std::span<RendererEvent>) override
        {
            return 0;
        }
        RendererState GetState() const override
        {
            return RendererState::RUNNING;
        }
        RendererInfo GetInfo() const override
        {
            return {API::VULKAN, 7};
        }
        void RequestShutdown() override
        {
        }
    } renderer;
    auto *actor = MeshActor();
    bridge.SetWorld(world.Get());
    auto        packet   = std::make_unique<FramePacket>(Sync());
    const auto *identity = packet.get();
    EXPECT_EQ(renderer.TrySubmit(packet).status, PacketSubmitStatus::BUSY);
    EXPECT_EQ(packet.get(), identity);
    actor->GetComponent<TransformComponent>()->SetLocalTranslation({9, 0, 0});
    renderer.next = PacketSubmitStatus::REJECTED;
    EXPECT_EQ(renderer.TrySubmit(packet).status, PacketSubmitStatus::REJECTED);
    EXPECT_EQ(packet.get(), identity);
    EXPECT_EQ(Commands<CreateRenderEntity>(*packet).front().desc.world.v[12], 0);
    renderer.next = PacketSubmitStatus::ACCEPTED;
    EXPECT_EQ(renderer.TrySubmit(packet).status, PacketSubmitStatus::ACCEPTED);
    EXPECT_FALSE(packet);
    const auto updates = Commands<UpdateRenderEntity>(Sync());
    ASSERT_EQ(updates.size(), 1u);
    EXPECT_EQ(updates.front().desc.world.v[12], 9);
}

TEST(SceneMeshConversion, PendingAndWrongTypeAssetsReturnExplicitly)
{
    auto pending = std::make_shared<Asset<Mesh>>();
    pending->SetType(Name(AssetTraits<Mesh>::ASSET_TYPE.data()));
    auto source = CreateSceneAssetSource({}, [pending](const Uuid &) { return pending; });
    EXPECT_TRUE(std::holds_alternative<std::monostate>(source->ResolveMesh(Uuid::CreateWithSeed(111), {}, {API::VULKAN, 1})));
    auto wrong = std::make_shared<Asset<Material>>();
    wrong->SetType(Name(AssetTraits<Material>::ASSET_TYPE.data()));
    source = CreateSceneAssetSource({}, [wrong](const Uuid &) { return wrong; });
    EXPECT_TRUE(std::holds_alternative<RendererError>(source->ResolveMesh(Uuid::CreateWithSeed(111), {}, {API::VULKAN, 1})));
}

TEST_F(SceneBridgeTest, OwnedGeometrySnapshotTracksParentsAndSurvivesWorldRetirement)
{
    auto *parent = world->CreateActor();
    auto *actor  = MeshActor();
    actor->GetComponent<TransformComponent>()->SetParentPreserveLocal(parent->GetComponent<TransformComponent>());
    bridge.EnableGeometrySnapshots(true);
    bridge.SetWorld(world.Get());
    Sync();
    const auto first = bridge.GeometrySnapshot();
    ASSERT_TRUE(first);
    ASSERT_EQ(first->instances.size(), 1u);
    EXPECT_EQ(first->instances[0].actor, actor->GetUuid());
    EXPECT_EQ(first->instances[0].state, SceneGeometryState::READY);
    ASSERT_TRUE(first->instances[0].resources);
    Sync();
    EXPECT_EQ(bridge.GeometrySnapshot()->revision, first->revision);
    EXPECT_EQ(bridge.GeometrySnapshot(), first);
    parent->GetComponent<TransformComponent>()->SetLocalTranslation({2, 3, 4});
    Sync();
    EXPECT_GT(bridge.GeometrySnapshot()->revision, first->revision);
    EXPECT_EQ(bridge.GeometrySnapshot()->instances[0].world.v[12], 2);
    EXPECT_EQ(bridge.GeometrySnapshot()->baseVersion, first->version);
    EXPECT_EQ(bridge.GeometrySnapshot()->changed.size(), 1u);
    EXPECT_EQ(bridge.GeometrySnapshot()->instances[0].generation, first->instances[0].generation);
    EXPECT_EQ(bridge.GeometrySnapshot()->instances[0].geometryRevision, first->instances[0].geometryRevision);
    EXPECT_GT(bridge.GeometrySnapshot()->instances[0].transformRevision, first->instances[0].transformRevision);
    EXPECT_EQ(first->instances[0].world.v[12], 0);
    bridge.SetWorld(nullptr);
    world.Reset(nullptr);
    EXPECT_FALSE(bridge.GeometrySnapshot());
    EXPECT_EQ(first->instances[0].resources->payloads[0].bytes.size(), 4u);
}

TEST_F(SceneBridgeTest, PendingReadinessDoesNotInvalidateUnchangedClickContext)
{
    MeshActor();
    source->pending = true;
    bridge.EnableGeometrySnapshots(true);
    bridge.SetWorld(world.Get());
    Sync();
    const auto pending = bridge.GeometrySnapshot();
    ASSERT_EQ(pending->instances.size(), 1u);
    EXPECT_EQ(pending->instances[0].state, SceneGeometryState::PENDING);
    source->pending = false;
    Sync();
    EXPECT_EQ(bridge.GeometrySnapshot()->revision, pending->revision);
    EXPECT_EQ(bridge.GeometrySnapshot()->instances[0].state, SceneGeometryState::READY);
    EXPECT_EQ(bridge.GeometrySnapshot()->instances[0].geometryRevision, pending->instances[0].geometryRevision);
    EXPECT_GT(bridge.GeometrySnapshot()->instances[0].readinessRevision, pending->instances[0].readinessRevision);
}

TEST(SceneMeshConversion, ExplicitInvalidationReloadsOwnedAssetSource)
{
    int  loads   = 0;
    auto pending = std::make_shared<Asset<Mesh>>();
    pending->SetType(Name(AssetTraits<Mesh>::ASSET_TYPE.data()));
    auto       source = CreateSceneAssetSource({}, [&](const Uuid &) {
        ++loads;
        return pending;
    });
    const auto id     = Uuid::CreateWithSeed(700);
    source->ResolveMesh(id, {}, {API::VULKAN, 1});
    source->ResolveMesh(id, {}, {API::VULKAN, 1});
    EXPECT_EQ(loads, 1);
    const auto revision = source->Revision();
    EXPECT_TRUE(source->Invalidate(id));
    EXPECT_GT(source->Revision(), revision);
    source->ResolveMesh(id, {}, {API::VULKAN, 1});
    EXPECT_EQ(loads, 2);
    AsseEvent::BroadCast(id, &IAssetEvent::OnAssetReload);
    source->ResolveMesh(id, {}, {API::VULKAN, 1});
    EXPECT_EQ(loads, 3);
}

TEST_F(SceneBridgeTest, HighlightMappingUsesLiveIdentityAndSurfaceContract)
{
    source->staticOpaque          = true;
    auto                   *actor = MeshActor();
    const std::vector<Uuid> ids{actor->GetUuid()};
    bridge.SetWorld(world.Get());
    bridge.EnableGeometrySnapshots(true);
    const auto packet  = Sync();
    const auto created = Commands<CreateRenderEntity>(packet);
    ASSERT_EQ(created.size(), 1u);
    EXPECT_EQ(bridge.Entities(ids, true), (std::vector<RenderEntityRef>{created.front().id}));
    source->staticOpaque = false;
    Sync();
    EXPECT_TRUE(bridge.Entities(ids, true).empty());
    EXPECT_EQ(bridge.Entities(ids).size(), 1u);
    actor->AddComponent<DirectLightComponent>();
    source->staticOpaque = true;
    Sync();
    EXPECT_EQ(bridge.Entities(ids).size(), 2u);
    EXPECT_EQ(bridge.Entities(ids, true), (std::vector<RenderEntityRef>{created.front().id}));
    bridge.SetWorld(nullptr);
    EXPECT_TRUE(bridge.Entities(ids).empty());
}
