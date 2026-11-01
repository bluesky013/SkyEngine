//
// Created on 2026/10/30.
//

#include <recast/render/NavigationRenderBridge.h>

#include <aurora/adaptor/renderer/SceneAssetSource.h>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::ai;
using namespace sky::aurora;

namespace {
    class MaterialSource final : public ISceneResourceSource {
    public:
        SceneResourceResult ResolveMesh(const Uuid &mesh, const Uuid &, RendererInfo info) override
        {
            EXPECT_EQ(mesh, ScenePrimitiveId(ScenePrimitive::PLANE));
            auto                  resources = std::make_shared<SceneMeshResources>();
            RenderResourcePayload material;
            material.resource           = {Uuid::Create(), 1, 1, info.deviceEpoch};
            material.format             = Name("runtime-material");
            auto &technique             = material.material.emplace().techniques.emplace_back();
            technique.tag               = Name("opaque");
            technique.parameters.values = {{Name("gMaterial.tint"), MakeShaderData(Vector4{1, 1, 1, 1})}};
            resources->payloads.push_back(std::move(material));
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
    ASSERT_TRUE(bridge.Append(packet, SLOT, INFO, geometry, resources));
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
    EXPECT_TRUE(bridge.Append(unchanged, SLOT, INFO, Triangle(), resources));
    EXPECT_TRUE(unchanged.commands.empty());
    FramePacket updated;
    EXPECT_TRUE(bridge.Append(updated, SLOT, INFO, Triangle(2), resources));
    EXPECT_EQ(Count<UpdateRenderEntity>(updated), 1u);
    EXPECT_EQ(Count<ResourceCpuReady>(updated), 1u);
    EXPECT_EQ(updated.releasedResources.size(), 1u);
}

TEST(NavigationRenderBridgeTest, DisableWaitsForRetirementBeforeRecreatingEntity)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    FramePacket            create;
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources));
    FramePacket hide;
    bridge.Clear(hide);
    EXPECT_EQ(Count<DestroyRenderEntity>(hide), 1u);
    EXPECT_EQ(hide.releasedResources.size(), 2u);
    FramePacket waiting;
    EXPECT_TRUE(bridge.Append(waiting, SLOT, INFO, Triangle(), resources));
    EXPECT_TRUE(waiting.commands.empty());
    bridge.OnEvent(EntityDestroyed{SLOT});
    FramePacket recreate;
    ASSERT_TRUE(bridge.Append(recreate, SLOT, INFO, Triangle(), resources));
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
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources));
    const RenderEntityRef next{{2, 1}, {8193, 1}};
    FramePacket           change;
    EXPECT_TRUE(bridge.Append(change, next, INFO, Triangle(), resources));
    EXPECT_EQ(Count<DestroyRenderEntity>(change), 1u);
    EXPECT_EQ(Count<CreateRenderEntity>(change), 0u);
    bridge.OnEvent(EntityDestroyed{SLOT});
    FramePacket recreate;
    ASSERT_TRUE(bridge.Append(recreate, next, INFO, Triangle(), resources));
    EXPECT_EQ(Count<CreateRenderEntity>(recreate), 1u);
}

TEST(NavigationRenderBridgeTest, RejectsInvalidTrianglesAndRecoversOnNewRenderer)
{
    NavigationRenderBridge bridge;
    MaterialSource         resources;
    auto                   invalid      = Triangle();
    invalid.vertices.front().position.x = std::numeric_limits<float>::quiet_NaN();
    FramePacket rejected;
    EXPECT_FALSE(bridge.Append(rejected, SLOT, INFO, invalid, resources));
    EXPECT_TRUE(rejected.commands.empty());
    invalid = Triangle();
    invalid.vertices.pop_back();
    EXPECT_FALSE(bridge.Append(rejected, SLOT, INFO, invalid, resources));
    FramePacket create;
    ASSERT_TRUE(bridge.Append(create, SLOT, INFO, Triangle(), resources));
    bridge.OnRendererStopped();
    FramePacket restarted;
    ASSERT_TRUE(bridge.Append(restarted, SLOT, {API::VULKAN, 18}, Triangle(), resources));
    EXPECT_EQ(Count<CreateRenderEntity>(restarted), 1u);
    EXPECT_EQ(Count<ResourceCpuReady>(restarted), 2u);
}
