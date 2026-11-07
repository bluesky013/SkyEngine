#include <aurora/adaptor/renderer/RenderSceneBridge.h>
#include <gtest/gtest.h>
#include <particle/render/ParticleRenderFeature.h>
#include <particle/render/ParticleRenderPayload.h>

#include <cstring>
#include <limits>

using namespace sky;
using namespace sky::particle;
using namespace sky::aurora;

namespace {
    class Images final : public ISceneResourceSource {
    public:
        SceneResourceResult ResolveMesh(const Uuid &, const Uuid &, RendererInfo) override
        {
            return {};
        }
        SceneImageResult ResolveImage(const Uuid &id, RendererInfo info, SceneImageUsage usage) override
        {
            EXPECT_EQ(usage, SceneImageUsage::MATERIAL);
            if (fail)
                return RendererError{RendererErrorCode::INVALID_ARGUMENT, "Injected image failure"};
            if (pending)
                return {};
            auto payload            = std::make_shared<RenderResourcePayload>();
            payload->resource       = {id, revision, 3, info.deviceEpoch};
            payload->format         = Name("material-image");
            payload->metadata.image = {1, 1, 1, 1, RenderImageFormat::RGBA8_UNORM};
            payload->bytes          = {255, 255, 255, 255};
            return std::shared_ptr<const RenderResourcePayload>(std::move(payload));
        }
        uint64_t revision = 1;
        bool     pending  = false;
        bool     fail     = false;
    };
    std::shared_ptr<ParticleRenderFrame> Frame(Uuid world, Uuid texture, float x)
    {
        auto frame   = std::make_shared<ParticleRenderFrame>();
        frame->world = world;
        ParticleRenderEmitter emitter;
        emitter.texture = texture;
        ParticleRenderInstance particle;
        particle.position.x = x;
        emitter.instances.push_back(particle);
        frame->emitters.push_back(std::move(emitter));
        return frame;
    }
} // namespace

TEST(ParticleRender, FreezesPerWorldDataAndExactTextureVersionsInEachPacket)
{
    auto                 feature = std::make_shared<ParticleRenderFeature>();
    ParticleRenderSource source(feature, {9, 1});
    auto                 images = std::make_shared<Images>();
    const auto           worldA = Uuid::Create(), worldB = Uuid::Create(), textureA = Uuid::Create(), textureB = Uuid::Create();
    source.Submit(Frame(worldA, textureA, 1.f));
    source.Submit(Frame(worldB, textureB, 2.f));
    const std::array<SceneExtensionSourceView, 2> views{{{worldA, {0, 1}, {1, 0, 0}}, {worldB, {1, 1}, {2, 0, 0}}}};
    SceneExtensionSourceContext                   context{{1, 1}, 1, {API::VULKAN, 1}, images, views};
    FramePacket                                   first;
    ASSERT_FALSE(source.Append(first, context));
    ASSERT_EQ(first.extensions.size(), 2u);
    EXPECT_EQ(first.extensions[0].outputs.front(), views[0].output);
    EXPECT_EQ(first.extensions[1].resources.front().product, textureB);
    const auto bytes = first.extensions[0].bytes;
    source.Submit(Frame(worldA, textureA, 20.f));
    images->revision = 2;
    FramePacket second;
    ASSERT_FALSE(source.Append(second, context));
    ASSERT_EQ(second.extensions.size(), 2u);
    EXPECT_TRUE(second.featureCommands.empty());
    EXPECT_EQ(first.extensions[0].bytes, bytes);
    EXPECT_NE(second.extensions[0].bytes, bytes);
    EXPECT_EQ(first.extensions[0].resources.front().revision, 1u);
    EXPECT_EQ(second.extensions[0].resources.front().revision, 2u);
    EXPECT_EQ(second.releasedResources.size(), 2u);
    ParticleViewInfo view;
    ASSERT_TRUE(source.GetView(worldA, view));
    EXPECT_FLOAT_EQ(view.position.x, 1.f);
    ASSERT_TRUE(source.GetView(worldB, view));
    EXPECT_FLOAT_EQ(view.position.x, 2.f);
    source.RemoveWorld(worldA);
    FramePacket removed;
    ASSERT_FALSE(source.Append(removed, context));
    ASSERT_EQ(removed.extensions.size(), 1u);
    EXPECT_EQ(removed.extensions[0].outputs.front(), views[1].output);
    ASSERT_EQ(removed.releasedResources.size(), 1u);
    EXPECT_EQ(removed.releasedResources[0].product, textureA);
}

TEST(ParticleRender, OmitsPendingTexturesAndPublishesUntexturedFallbackOnce)
{
    auto                 feature = std::make_shared<ParticleRenderFeature>();
    ParticleRenderSource source(feature, {9, 1});
    auto                 images = std::make_shared<Images>();
    images->pending             = true;
    const auto world            = Uuid::Create();
    source.Submit(Frame(world, Uuid::Create(), 1.f));
    const std::array<SceneExtensionSourceView, 1> views{{{world, {0, 1}, {}}}};
    SceneExtensionSourceContext                   context{{1, 1}, 1, {API::VULKAN, 1}, images, views};
    FramePacket                                   pending;
    ASSERT_FALSE(source.Append(pending, context));
    EXPECT_TRUE(pending.extensions.empty());
    EXPECT_TRUE(pending.commands.empty());
    source.Submit(Frame(world, {}, 1.f));
    FramePacket white;
    ASSERT_FALSE(source.Append(white, context));
    ASSERT_EQ(white.extensions.size(), 1u);
    EXPECT_EQ(white.commands.size(), 1u);
    FramePacket unchanged;
    ASSERT_FALSE(source.Append(unchanged, context));
    EXPECT_TRUE(unchanged.commands.empty());
    EXPECT_TRUE(unchanged.releasedResources.empty());
    EXPECT_EQ(unchanged.extensions[0].resources, white.extensions[0].resources);
}

TEST(ParticleRender, RejectsMalformedPayloadsAndExclusiveBlendBoundary)
{
    ParticleRenderFeature                      feature;
    const auto                                 texture = Uuid::Create();
    const auto                                 frame   = Frame(Uuid::Create(), texture, 1.f);
    const std::array<ParticleRenderTexture, 1> textures{{{texture, {texture, 1, 3, 1}}}};
    auto                                       encoded = EncodeParticleRenderFrame(*frame, {0, 1}, textures);
    ASSERT_TRUE(std::holds_alternative<FrameExtensionPayload>(encoded));
    auto payload = std::get<FrameExtensionPayload>(encoded);
    EXPECT_FALSE(feature.Validate(payload));
    payload.bytes.pop_back();
    EXPECT_TRUE(feature.Validate(payload));
    payload                 = std::get<FrameExtensionPayload>(encoded);
    const auto invalidBlend = static_cast<uint32_t>(ParticleBlendMode::END);
    std::memcpy(payload.bytes.data() + sizeof(uint32_t) * 2, &invalidBlend, sizeof(invalidBlend));
    EXPECT_TRUE(feature.Validate(payload));
    frame->emitters[0].instances[0].position.x = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(std::holds_alternative<RendererError>(EncodeParticleRenderFrame(*frame, {0, 1}, textures)));
}

TEST(ParticleRender, FailedAppendPreservesPacketAndCanRetryRegistration)
{
    auto                 feature = std::make_shared<ParticleRenderFeature>();
    ParticleRenderSource source(feature, {9, 1});
    auto                 images = std::make_shared<Images>();
    images->fail                = true;
    const auto world            = Uuid::Create();
    source.Submit(Frame(world, Uuid::Create(), 1.f));
    const std::array<SceneExtensionSourceView, 1> views{{{world, {0, 1}, {}}}};
    SceneExtensionSourceContext                   context{{1, 1}, 1, {API::VULKAN, 1}, images, views};
    FramePacket                                   packet;
    packet.extensions.push_back({Name("existing"), {5}, {}, {}});
    ASSERT_TRUE(source.Append(packet, context));
    EXPECT_EQ(packet.extensions.size(), 1u);
    EXPECT_TRUE(packet.commands.empty());
    EXPECT_TRUE(packet.featureCommands.empty());
    ParticleViewInfo view;
    EXPECT_FALSE(source.GetView(world, view));
    images->fail = false;
    ASSERT_FALSE(source.Append(packet, context));
    EXPECT_EQ(packet.featureCommands.size(), 1u);
    EXPECT_EQ(packet.commands.size(), 1u);
    EXPECT_EQ(packet.extensions.size(), 2u);
    context.renderer.deviceEpoch = 2;
    FramePacket restarted;
    ASSERT_FALSE(source.Append(restarted, context));
    EXPECT_EQ(restarted.featureCommands.size(), 1u);
    EXPECT_EQ(restarted.commands.size(), 1u);
    EXPECT_TRUE(restarted.releasedResources.empty());
}


TEST(ParticleRender, ModuleRetirementRequestsHostFlush)
{
    auto                 feature = std::make_shared<ParticleRenderFeature>();
    ParticleRenderSource source(feature, {9, 42});
    auto                 images = std::make_shared<Images>();
    const auto           world  = Uuid::Create();
    source.Submit(Frame(world, {}, 1.f));
    const std::array<SceneExtensionSourceView, 1> views{{{world, {0, 1}, {}}}};
    SceneExtensionSourceContext                   context{{1, 1}, 1, {API::VULKAN, 1}, images, views};
    FramePacket                                   packet;
    ASSERT_FALSE(source.Append(packet, context));
    std::vector<SceneExtensionRetirement> pending;
    SceneExtensionRetirementQueue::Get()->Drain(pending);
    pending.clear();
    source.RequestRetire();
    SceneExtensionRetirementQueue::Get()->Drain(pending);
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].extension, (RenderFeatureId{9, 42}));
    EXPECT_EQ(pending[0].scene, (RenderFeatureId{1, 1}));
}
