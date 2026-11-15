#include "EditorPreviewSessions.h"
#include "SceneAssetFixtures.h"

#include <algorithm>
#include <aurora/adaptor/components/DirectLightComponent.h>
#include <aurora/adaptor/components/StaticMeshComponent.h>
#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <framework/application/ModuleManager.h>
#include <framework/world/TransformComponent.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::editor;
using namespace sky::aurora;

namespace {
    class PreviewMetadata final : public testing::Environment {
    public:
        void SetUp() override
        {
            AssetManager::Get()->SetSourceCatalog(&catalog);
            modules = std::make_unique<ModuleManager>();
            ASSERT_TRUE(modules->RegisterModule({"Aurora.Render", {"Aurora.Metadata"}}));
            ASSERT_TRUE(modules->TopoSort());
            StartArguments args;
            args.args = {"EditorRenderTest", "--aurora-provider-only"};
            ASSERT_TRUE(modules->LoadModules(args));
            ASSERT_TRUE(modules->StartModules());
        }
        void TearDown() override
        {
            modules.reset();
            AssetManager::Get()->SetSourceCatalog(nullptr);
        }

    private:
        SceneFixtureCatalog            catalog;
        std::unique_ptr<ModuleManager> modules;
    };
    const auto *previewMetadata = testing::AddGlobalTestEnvironment(new PreviewMetadata());
    struct EmptySource : ISceneResourceSource {
        SceneResourceResult ResolveMesh(const Uuid &, const Uuid &, RendererInfo) override
        {
            return {};
        }
    };
    struct StageSource final : ISceneResourceSource {
        Uuid                subject = Uuid::Create();
        AABB                local{{-1.f, 2.f, -2.f}, {3.f, 6.f, 2.f}};
        std::optional<AABB> MeshBounds(const Uuid &id) const override
        {
            return id == subject ? std::optional{local} : std::nullopt;
        }
        SceneResourceResult ResolveMesh(const Uuid &id, const Uuid &material, RendererInfo info) override
        {
            auto                    result = std::make_shared<SceneMeshResources>();
            const RenderResourceKey meshKey{id, 1, 0, info.deviceEpoch},
                materialKey{material ? material : SceneLitMaterialId(), 1, 0, info.deviceEpoch};
            result->parts.push_back({.mesh = {.resource = meshKey}, .material = {.resource = materialKey}});
            result->payloads.push_back({.resource = meshKey});
            result->payloads.push_back({.resource = materialKey});
            return result;
        }
    };
    template <typename T>
    std::vector<T> Commands(const FramePacket &packet)
    {
        std::vector<T> values;
        for (const auto &command : packet.commands)
            if (const auto *value = std::get_if<T>(&command))
                values.push_back(*value);
        return values;
    }
    class WorldLifetimeProbe final : public IWorldSubSystem {
    public:
        explicit WorldLifetimeProbe(std::shared_ptr<bool> destroyed) : destroyed(std::move(destroyed))
        {
        }
        ~WorldLifetimeProbe() override
        {
            *destroyed = true;
        }

    private:
        std::shared_ptr<bool> destroyed;
    };
    WorldPtr TrackedWorld(std::shared_ptr<bool> destroyed)
    {
        WorldPtr world = World::CreateWorld();
        world->AddSubSystem(Name("LifetimeProbe"), new WorldLifetimeProbe(std::move(destroyed)));
        return world;
    }
    struct PreviewSessions : testing::Test {
        uint32_t              next = 10;
        UiSnapshotProducer    textures{1};
        EditorPreviewSessions host{{API::VULKAN, 1}, std::make_shared<EmptySource>(), textures, [this] { return ViewportId{next++, 1}; }};
        void                  SetUp() override
        {
            ASSERT_TRUE(host.SetGroundAssets(ScenePrimitiveId(ScenePrimitive::PLANE), SceneLitMaterialId()));
        }
        AssetPreviewHandle Create(WorldPtr world = World::CreateWorld())
        {
            AssetPreviewScene scene;
            scene.world       = std::move(world);
            const auto result = host.CreateSession(std::move(scene));
            EXPECT_TRUE(std::holds_alternative<AssetPreviewHandle>(result));
            return std::get<AssetPreviewHandle>(result);
        }
    };
} // namespace
TEST_F(PreviewSessions, TwoSessionsHaveIndependentSceneViewOutputAndResize)
{
    const auto  first = Create(), second = Create();
    FramePacket create;
    create.sequence = 1;
    ASSERT_FALSE(host.Append(create));
    const auto scenes  = Commands<CreateScene>(create);
    const auto views   = Commands<CreateView>(create);
    const auto outputs = Commands<CreateViewport>(create);
    ASSERT_EQ(scenes.size(), 2u);
    ASSERT_EQ(views.size(), 2u);
    ASSERT_EQ(outputs.size(), 2u);
    EXPECT_NE(scenes[0].id, scenes[1].id);
    EXPECT_NE(views[0].id, views[1].id);
    EXPECT_NE(outputs[0].id, outputs[1].id);
    const auto firstStatus  = std::get<AssetPreviewStatus>(host.Status(first));
    const auto secondStatus = std::get<AssetPreviewStatus>(host.Status(second));
    EXPECT_NE(firstStatus.texture, secondStatus.texture);
    EXPECT_EQ(firstStatus.state, AssetPreviewState::Pending);
    host.OnEvent(FrameCompleted{1, 1});
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(first)).state, AssetPreviewState::Ready);
    host.Resize(first, 512, 384);
    FramePacket resize;
    resize.sequence = 2;
    ASSERT_FALSE(host.Append(resize));
    const auto resized = Commands<ResizeViewport>(resize);
    EXPECT_EQ(Commands<UpdateView>(resize).size(), 1u);
    ASSERT_EQ(resized.size(), 1u);
    EXPECT_EQ(resized[0].id, outputs[0].id);
    EXPECT_EQ(resized[0].extent.width, 512u);
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(second)).revision, 0u);
    auto stale       = first;
    stale.generation = Uuid::Create();
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.Resize(stale, 100, 100)));
    EditorCamera camera;
    camera.nearPlane = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.Update(first, camera)));
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.Resize(first, 0, 100)));
}
TEST_F(PreviewSessions, OnlyCameraOrExtentChangesReviseTheView)
{
    const auto  handle = Create();
    FramePacket create;
    create.sequence = 1;
    ASSERT_FALSE(host.Append(create));
    EditorCamera camera;
    host.Update(handle, camera);
    FramePacket idle;
    idle.sequence = 2;
    ASSERT_FALSE(host.Append(idle));
    EXPECT_TRUE(Commands<UpdateView>(idle).empty());
    camera.yaw += .2f;
    host.Update(handle, camera);
    host.Update(handle, camera);
    FramePacket orbit;
    orbit.sequence = 3;
    ASSERT_FALSE(host.Append(orbit));
    ASSERT_EQ(Commands<UpdateView>(orbit).size(), 1u);
    EXPECT_TRUE(Commands<ResizeViewport>(orbit).empty());
    FramePacket unchanged;
    unchanged.sequence = 4;
    ASSERT_FALSE(host.Append(unchanged));
    EXPECT_TRUE(Commands<UpdateView>(unchanged).empty());
}
TEST_F(PreviewSessions, CloseRetainsWorldUntilEveryRendererObjectRetires)
{
    auto        destroyed = std::make_shared<bool>(false);
    const auto  handle    = Create(TrackedWorld(destroyed));
    FramePacket create;
    create.sequence = 1;
    ASSERT_FALSE(host.Append(create));
    const auto scene  = Commands<CreateScene>(create).front().id;
    const auto view   = Commands<CreateView>(create).front().id;
    const auto output = Commands<CreateViewport>(create).front().id;
    host.CloseSession(handle);
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(handle)).texture, 0u);
    FramePacket close;
    close.sequence = 2;
    ASSERT_FALSE(host.Append(close));
    EXPECT_EQ(Commands<DestroyView>(close).size(), 1u);
    EXPECT_EQ(Commands<DestroyScene>(close).size(), 1u);
    EXPECT_EQ(Commands<DestroyViewport>(close).size(), 1u);
    host.OnEvent(ViewportDestroyed{{output.index, output.generation + 1}});
    EXPECT_FALSE(*destroyed);
    host.OnEvent(SceneDestroyed{scene});
    host.OnEvent(ViewDestroyed{view});
    EXPECT_FALSE(*destroyed);
    host.OnEvent(ViewportDestroyed{output});
    EXPECT_TRUE(*destroyed);
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(handle)).state, AssetPreviewState::Closed);
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.Update(handle, {})));
}
TEST_F(PreviewSessions, UnsubmittedCloseAndStoppedHostReleaseSessionsWithoutGpuClaims)
{
    auto       destroyed = std::make_shared<bool>(false);
    const auto handle    = Create(TrackedWorld(destroyed));
    host.CloseSession(handle);
    FramePacket packet;
    packet.sequence = 1;
    ASSERT_FALSE(host.Append(packet));
    EXPECT_TRUE(packet.commands.empty());
    EXPECT_TRUE(*destroyed);
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(handle)).state, AssetPreviewState::Closed);
    const auto second = Create();
    host.OnRendererStopped();
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(second)).state, AssetPreviewState::Closed);
    AssetPreviewScene scene;
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.CreateSession(std::move(scene))));
}

TEST_F(PreviewSessions, GroundAndStudioLightsBelongOnlyToTheirPreview)
{
    ASSERT_TRUE(host.SetResources(std::make_shared<StageSource>()));
    const WorldPtr    world = World::CreateWorld();
    AssetPreviewScene scene;
    scene.world           = world;
    scene.subjectBounds   = AABB{{4.f, -2.f, -5.f}, {6.f, 1.f, -1.f}};
    const auto withGround = host.CreateSession(scene);
    ASSERT_TRUE(std::holds_alternative<AssetPreviewHandle>(withGround));
    scene.showGround = false;
    ASSERT_TRUE(std::holds_alternative<AssetPreviewHandle>(host.CreateSession(scene)));
    EXPECT_TRUE(world->GetActors().empty());
    FramePacket frame;
    frame.sequence = 1;
    ASSERT_FALSE(host.Append(frame));
    const auto scenes = Commands<CreateScene>(frame);
    ASSERT_EQ(scenes.size(), 2u);
    const auto entities = Commands<CreateRenderEntity>(frame);
    ASSERT_EQ(entities.size(), 3u);
    size_t receivers = 0, key = 0, fill = 0;
    for (const auto &entity : entities) {
        EXPECT_EQ(entity.id.scene, scenes.front().id);
        if (entity.desc.mesh.resource.product) {
            ++receivers;
            EXPECT_EQ(entity.desc.mesh.resource.product, ScenePrimitiveId(ScenePrimitive::PLANE));
            EXPECT_EQ(entity.desc.material.resource.product, SceneLitMaterialId());
            EXPECT_FLOAT_EQ(entity.desc.world.v[12], 5.f);
            EXPECT_LT(entity.desc.world.v[13], -2.f);
            EXPECT_GT(entity.desc.world.v[13], -2.01f);
            EXPECT_FLOAT_EQ(entity.desc.world.v[14], -3.f);
            EXPECT_FALSE(entity.desc.properties.castShadow);
            EXPECT_TRUE(entity.desc.properties.receiveShadow);
        } else {
            EXPECT_EQ(entity.desc.properties.light.kind, RenderLightKind::DIRECTIONAL);
            entity.desc.properties.light.castShadow ? ++key : ++fill;
        }
    }
    EXPECT_EQ(receivers, 1u);
    EXPECT_EQ(key, 1u);
    EXPECT_EQ(fill, 1u);
    EXPECT_TRUE(std::get<AssetPreviewStatus>(host.Status(std::get<AssetPreviewHandle>(withGround))).bounds.empty());
    EXPECT_TRUE(world->GetActors().empty());
    scene.subjectBounds->max.y = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.CreateSession(scene)));
}

TEST_F(PreviewSessions, GroundFitsFirstSubjectBoundsOnceAndKeepsAuthoredLighting)
{
    const auto source = std::make_shared<StageSource>();
    ASSERT_TRUE(host.SetResources(source));
    const WorldPtr world   = World::CreateWorld();
    auto          *subject = world->CreateActor("Subject");
    ASSERT_NE(subject, nullptr);
    auto *mesh = subject->AddComponent<StaticMeshComponent>();
    ASSERT_NE(mesh, nullptr);
    mesh->SetMeshUuid(source->subject);
    auto *transform = subject->GetComponent<TransformComponent>();
    ASSERT_NE(transform, nullptr);
    transform->SetLocalTranslation({0.f, 3.f, 0.f});
    auto *sceneLight = world->CreateActor("Scene light");
    ASSERT_NE(sceneLight, nullptr);
    ASSERT_NE(sceneLight->AddComponent<DirectLightComponent>(), nullptr);
    const auto  handle = Create(world);
    FramePacket first;
    first.sequence = 1;
    ASSERT_FALSE(host.Append(first));
    const auto initial = Commands<CreateRenderEntity>(first);
    EXPECT_EQ(std::count_if(initial.begin(), initial.end(), [](const auto &entity) { return !entity.desc.mesh.resource.product; }), 1);
    const auto bounds = std::get<AssetPreviewStatus>(host.Status(handle)).bounds;
    ASSERT_EQ(bounds.size(), 1u);
    EXPECT_FLOAT_EQ(bounds.front().min.y, 5.f);
    FramePacket settled;
    settled.sequence = 2;
    ASSERT_FALSE(host.Append(settled));
    const auto created = Commands<CreateRenderEntity>(settled);
    ASSERT_EQ(created.size(), 1u);
    EXPECT_EQ(created.front().desc.mesh.resource.product, ScenePrimitiveId(ScenePrimitive::PLANE));
    EXPECT_NEAR(created.front().desc.world.v[13], 5.f, .01f);
    transform->SetLocalTranslation({0.f, 6.f, 0.f});
    FramePacket moved;
    moved.sequence = 3;
    ASSERT_FALSE(host.Append(moved));
    for (const auto &update : Commands<UpdateRenderEntity>(moved))
        EXPECT_NE(update.id, created.front().id);
    EXPECT_EQ(world->GetActors().size(), 2u);
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(handle)).bounds.size(), 1u);
}
