//
// Created on 2026/10/30.
//

#include <recast/render/NavigationRenderBridge.h>

#include <cstring>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::ai;
using namespace sky::aurora;

namespace {
    class MaterialSource final : public ISceneResourceSource {
    public:
        Uuid     materialId = Uuid::Create();
        Uuid     textureId  = Uuid::Create();
        uint64_t revision   = 1;
        bool     textured = false, pending = false, failed = false;
        uint32_t loads = 0;
        uint64_t Revision() const override
        {
            return revision;
        }
        SceneResourceResult ResolveMesh(const Uuid &, const Uuid &, RendererInfo) override
        {
            ADD_FAILURE() << "Navigation must not load mesh assets to resolve its material";
            return RendererError{RendererErrorCode::UNSUPPORTED, "No mesh assets available"};
        }
        SceneMaterialResult ResolveMaterial(const Uuid &id, RendererInfo info, VertexSemanticMask mask, bool skeletal) override
        {
            ++loads;
            EXPECT_EQ(id, materialId);
            EXPECT_EQ(mask.mask, 1u << uint32_t(VertexSemantic::POSITION));
            EXPECT_FALSE(skeletal);
            if (pending)
                return {};
            if (failed)
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Material failed"};
            auto  resources             = std::make_shared<SceneMaterialResources>();
            auto &material              = resources->material;
            material.resource           = {materialId, revision, 1, info.deviceEpoch};
            material.format             = Name("runtime-material");
            auto &technique             = material.material.emplace().techniques.emplace_back();
            technique.tag               = Name("opaque");
            technique.parameters.values = {{Name("gMaterial.tint"), MakeShaderData(Vector4{1, 1, 1, 1})}};
            if (textured) {
                RenderResourcePayload image;
                image.resource = {textureId, revision, 3, info.deviceEpoch};
                image.bytes    = {128, 64, 32, 255};
                resources->dependencies.push_back(std::make_shared<const RenderResourcePayload>(std::move(image)));
            }
            return resources;
        }
    };
    NaviDebugGeometry Triangle(float x = 0)
    {
        return {{{{x, 0, 0}, {}}, {{x + 1, 0, 0}, {}}, {{x, 0, 1}, {}}}};
    }
    template <class T>
    size_t Count(const FramePacket &packet)
    {
        return std::ranges::count_if(packet.commands, [](const auto &command) { return std::holds_alternative<T>(command); });
    }
    constexpr RendererInfo    INFO{API::VULKAN, 17};
    constexpr RenderEntityRef SLOT{{0, 1}, {8192, 1}};
} // namespace

TEST(NavigationRenderBridgeTest, EmitsOwnedTrianglePayloadAndReusesUnchangedGeometry)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    auto                   geometry = Triangle();
    FramePacket            packet;
    ASSERT_TRUE(bridge.Append(packet, SLOT, INFO, geometry, resources, resources.materialId));
    ASSERT_EQ(Count<CreateRenderEntity>(packet), 1u);
    EXPECT_EQ(Count<ResourceCpuReady>(packet), 2u);
    const auto entity =
        std::ranges::find_if(packet.commands, [](const auto &command) { return std::holds_alternative<CreateRenderEntity>(command); });
    ASSERT_NE(entity, packet.commands.end());
    const auto tags         = std::get<CreateRenderEntity>(*entity).desc.properties.tags;
    const auto ordinaryTags = RenderDrawProperties{}.tags;
    EXPECT_GT(tags, ordinaryTags);
    EXPECT_EQ(tags & ordinaryTags, ordinaryTags);
    const auto material = std::ranges::find_if(packet.commands, [](const auto &command) {
        const auto *ready = std::get_if<ResourceCpuReady>(&command);
        return ready && ready->payload.material.has_value();
    });
    ASSERT_NE(material, packet.commands.end());
    const auto &technique = std::get<ResourceCpuReady>(*material).payload.material->techniques.front();
    EXPECT_TRUE(technique.program.depth.test);
    EXPECT_FALSE(technique.program.depth.write);
    EXPECT_TRUE(technique.program.blend.blendEn);
    EXPECT_EQ(technique.program.blend.srcColor, BlendFactor::SRC_ALPHA);
    EXPECT_EQ(technique.program.blend.dstColor, BlendFactor::ONE_MINUS_SRC_ALPHA);
    const auto &values    = technique.parameters.values;
    const auto  tintValue = std::ranges::find(values, Name("gMaterial.tint"), &RenderMaterialValue::name);
    ASSERT_NE(tintValue, values.end());
    ASSERT_EQ(tintValue->value.bytes.size(), sizeof(Vector4));
    Vector4 tint;
    std::memcpy(&tint, tintValue->value.bytes.data(), sizeof(tint));
    EXPECT_FLOAT_EQ(tint.x, .1f);
    EXPECT_FLOAT_EQ(tint.y, .85f);
    EXPECT_FLOAT_EQ(tint.z, .25f);
    EXPECT_FLOAT_EQ(tint.w, .6f);
    const auto mesh = std::ranges::find_if(packet.commands, [](const auto &command) {
        const auto *ready = std::get_if<ResourceCpuReady>(&command);
        return ready && ready->payload.mesh.has_value();
    });
    ASSERT_NE(mesh, packet.commands.end());
    const auto &payload = std::get<ResourceCpuReady>(*mesh).payload;
    ASSERT_EQ(payload.bytes.size(), 3 * (sizeof(Vector3) + sizeof(uint32_t)));
    EXPECT_EQ(payload.mesh->lods.front().command.indexCount, 3u);
    geometry.vertices.clear();
    Vector3 position;
    std::memcpy(&position, payload.bytes.data(), sizeof(position));
    EXPECT_FLOAT_EQ(position.y, .02f);
    FramePacket unchanged;
    EXPECT_TRUE(bridge.Append(unchanged, SLOT, INFO, Triangle(), resources, resources.materialId));
    EXPECT_TRUE(unchanged.commands.empty());
    FramePacket updated;
    EXPECT_TRUE(bridge.Append(updated, SLOT, INFO, Triangle(2), resources, resources.materialId));
    EXPECT_EQ(Count<UpdateRenderEntity>(updated), 1u);
    EXPECT_EQ(Count<ResourceCpuReady>(updated), 1u);
    EXPECT_EQ(updated.releasedResources.size(), 1u);
}

TEST(NavigationRenderBridgeTest, DisableWaitsForRetirementBeforeRecreatingEntity)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    FramePacket            create;
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources, resources.materialId));
    FramePacket hide;
    bridge.Clear(hide);
    EXPECT_EQ(Count<DestroyRenderEntity>(hide), 1u);
    EXPECT_EQ(hide.releasedResources.size(), 2u);
    FramePacket waiting;
    EXPECT_TRUE(bridge.Append(waiting, SLOT, INFO, Triangle(), resources, resources.materialId));
    EXPECT_TRUE(waiting.commands.empty());
    bridge.OnEvent(EntityDestroyed{SLOT});
    FramePacket recreate;
    ASSERT_TRUE(bridge.Append(recreate, SLOT, INFO, Triangle(), resources, resources.materialId));
    const auto created =
        std::ranges::find_if(recreate.commands, [](const auto &command) { return std::holds_alternative<CreateRenderEntity>(command); });
    ASSERT_NE(created, recreate.commands.end());
    EXPECT_EQ(std::get<CreateRenderEntity>(*created).id.entity.generation, 2u);
}

TEST(NavigationRenderBridgeTest, SlotChangeRetiresOldEntityEvenForIdenticalGeometry)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    FramePacket            create;
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources, resources.materialId));
    const RenderEntityRef next{{2, 1}, {8193, 1}};
    FramePacket           change;
    EXPECT_TRUE(bridge.Append(change, next, INFO, Triangle(), resources, resources.materialId));
    EXPECT_EQ(Count<DestroyRenderEntity>(change), 1u);
    EXPECT_EQ(Count<CreateRenderEntity>(change), 0u);
    bridge.OnEvent(EntityDestroyed{SLOT});
    FramePacket recreate;
    ASSERT_TRUE(bridge.Append(recreate, next, INFO, Triangle(), resources, resources.materialId));
    EXPECT_EQ(Count<CreateRenderEntity>(recreate), 1u);
}

TEST(NavigationRenderBridgeTest, RejectsInvalidTrianglesAndRecoversOnNewRenderer)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    auto                   invalid      = Triangle();
    invalid.vertices.front().position.x = std::numeric_limits<float>::quiet_NaN();
    FramePacket rejected;
    EXPECT_FALSE(bridge.Append(rejected, SLOT, INFO, invalid, resources, resources.materialId));
    EXPECT_TRUE(rejected.commands.empty());
    invalid = Triangle();
    invalid.vertices.pop_back();
    EXPECT_FALSE(bridge.Append(rejected, SLOT, INFO, invalid, resources, resources.materialId));
    FramePacket create;
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources, resources.materialId));
    bridge.OnRendererStopped();
    FramePacket restarted;
    ASSERT_TRUE(bridge.Append(restarted, SLOT, {API::VULKAN, 18}, Triangle(), resources, resources.materialId));
    EXPECT_EQ(Count<CreateRenderEntity>(restarted), 1u);
    EXPECT_EQ(Count<ResourceCpuReady>(restarted), 2u);
}

TEST(NavigationRenderBridgeTest, MaterialOwnsDependenciesWithoutAnyPlaneAssetAndRefreshesWithSource)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    resources.textured = true;
    FramePacket create;
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources, resources.materialId));
    EXPECT_EQ(Count<ResourceCpuReady>(create), 3u);
    EXPECT_EQ(resources.loads, 1u);
    const auto oldTexture = std::get<ResourceCpuReady>(create.commands.front()).payload.resource;
    EXPECT_EQ(oldTexture.product, resources.textureId);
    ++resources.revision;
    FramePacket update;
    ASSERT_TRUE(bridge.Append(update, SLOT, INFO, Triangle(), resources, resources.materialId));
    EXPECT_EQ(Count<UpdateRenderEntity>(update), 1u);
    EXPECT_EQ(Count<ResourceCpuReady>(update), 3u);
    EXPECT_NE(std::ranges::find(update.releasedResources, oldTexture), update.releasedResources.end());
    EXPECT_EQ(resources.loads, 2u);
    FramePacket clear;
    bridge.Clear(clear);
    ASSERT_EQ(clear.releasedResources.size(), 3u);
    EXPECT_TRUE(std::ranges::any_of(clear.releasedResources,
                                    [&](const auto &key) { return key.product == resources.textureId && key.revision == resources.revision; }));
}

TEST(NavigationRenderBridgeTest, PendingOrFailedMaterialPublishesNoPartialResources)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    resources.pending = true;
    FramePacket packet;
    EXPECT_FALSE(bridge.Append(packet, SLOT, INFO, Triangle(), resources, resources.materialId));
    EXPECT_TRUE(packet.commands.empty());
    EXPECT_TRUE(bridge.GetError().empty());
    resources.pending = false;
    resources.failed  = true;
    EXPECT_FALSE(bridge.Append(packet, SLOT, INFO, Triangle(), resources, resources.materialId));
    EXPECT_TRUE(packet.commands.empty());
    EXPECT_EQ(bridge.GetError(), "Material failed");
    resources.failed = false;
    EXPECT_TRUE(bridge.Append(packet, SLOT, INFO, Triangle(), resources, resources.materialId));
}
