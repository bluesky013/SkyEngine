#include "graph/FrameFeatureRegistry.h"

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {
    class SceneProbe final : public IFrameGraphFeature {
    public:
        Name GetName() const override
        {
            return Name("scene");
        }
        bool SupportsSceneExtensions() const override
        {
            return true;
        }
        FrameOperationResult BuildFrame(FrameGraphBuildContext &, const FrameExtensionPayload &) override
        {
            return FrameOperationComplete{};
        }
        void OnRetired() override
        {
            ++retirements;
        }
        uint32_t retirements = 0;
    };
    class ExtensionProbe final : public ISceneRenderExtension {
    public:
        explicit ExtensionProbe(const std::string &name, uint32_t order = 0) : name(name.c_str()), order(order)
        {
        }
        Name GetName() const override
        {
            return name;
        }
        uint32_t GetOrder() const override
        {
            return order;
        }
        std::optional<RendererError> Validate(const FrameExtensionPayload &payload) const override
        {
            if (payload.bytes != std::vector<uint8_t>{42})
                return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid probe payload"};
            return {};
        }
        FrameOperationResult Build(FrameGraphBuildContext &, const SceneRenderStageContext &, const FrameExtensionPayload &) override
        {
            return FrameOperationComplete{};
        }
        void OnRetired() override
        {
            ++retirements;
        }
        Name     name;
        uint32_t order = 0, retirements = 0;
    };
    class SceneExtensionRegistryTest : public testing::Test {
    protected:
        void SetUp() override
        {
            FramePacket packet;
            packet.featureCommands.push_back(RegisterRenderFeature{{0, 1}, Name("scene"), scene});
            ASSERT_FALSE(registry.Validate(packet));
            registry.Apply(packet);
        }
        FrameFeatureRegistry        registry;
        std::shared_ptr<SceneProbe> scene = std::make_shared<SceneProbe>();
    };
} // namespace

TEST_F(SceneExtensionRegistryTest, RetiresIndependentlyAfterTheFinalFrameBorrower)
{
    auto        extension = std::make_shared<ExtensionProbe>("particle");
    FramePacket add;
    add.featureCommands.push_back(RegisterSceneExtension{{1, 1}, {0, 1}, extension});
    ASSERT_FALSE(registry.Validate(add));
    registry.Apply(add);
    auto borrowed = registry.SnapshotExtensions();
    ASSERT_EQ(borrowed.size(), 1u);
    FramePacket retire;
    retire.featureCommands.push_back(RetireSceneExtension{{1, 1}, {0, 1}});
    ASSERT_FALSE(registry.Validate(retire));
    registry.Apply(retire);
    EXPECT_FALSE(borrowed.front().CanBuild());
    EXPECT_TRUE(registry.SnapshotExtensions().empty());
    EXPECT_TRUE(registry.CollectRetired().empty());
    EXPECT_EQ(extension->retirements, 0u);
    borrowed.clear();
    EXPECT_EQ(registry.CollectRetired(), (std::vector<RenderFeatureId>{{1, 1}}));
    EXPECT_EQ(extension->retirements, 1u);
    EXPECT_EQ(scene->retirements, 0u);
    FramePacket replacement;
    replacement.featureCommands.push_back(RegisterSceneExtension{{1, 2}, {0, 1}, std::make_shared<ExtensionProbe>("particle")});
    EXPECT_FALSE(registry.Validate(replacement));
}

TEST_F(SceneExtensionRegistryTest, ValidatesCapacityAgainstEveryCommandInThePacket)
{
    FramePacket initial;
    for (uint32_t index = 1; index <= 63; ++index)
        initial.featureCommands.push_back(
            RegisterSceneExtension{{index, 1}, {0, 1}, std::make_shared<ExtensionProbe>("extension" + std::to_string(index))});
    ASSERT_FALSE(registry.Validate(initial));
    registry.Apply(initial);
    FramePacket overflow;
    overflow.featureCommands.push_back(RegisterSceneExtension{{64, 1}, {0, 1}, std::make_shared<ExtensionProbe>("extension64")});
    overflow.featureCommands.push_back(RegisterSceneExtension{{65, 1}, {0, 1}, std::make_shared<ExtensionProbe>("extension65")});
    ASSERT_TRUE(registry.Validate(overflow));
    EXPECT_EQ(registry.SnapshotExtensions().size(), 63u);
    overflow.featureCommands.pop_back();
    EXPECT_FALSE(registry.Validate(overflow));
}

TEST_F(SceneExtensionRegistryTest, RoutesOwnedPayloadsAndRejectsDuplicateNamesAndStaleRetirement)
{
    FramePacket packet;
    packet.featureCommands.push_back(RegisterSceneExtension{{1, 1}, {0, 1}, std::make_shared<ExtensionProbe>("particle")});
    packet.extensions.push_back({Name("particle"), {42}, {{0, 1}}, {}});
    ASSERT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    FramePacket duplicate;
    duplicate.featureCommands.push_back(RegisterSceneExtension{{2, 1}, {0, 1}, std::make_shared<ExtensionProbe>("particle")});
    EXPECT_TRUE(registry.Validate(duplicate));
    FramePacket stale;
    stale.featureCommands.push_back(RetireSceneExtension{{1, 2}, {0, 1}});
    EXPECT_TRUE(registry.Validate(stale));
    packet.extensions.front().bytes.front() = 0;
    EXPECT_TRUE(registry.Validate(packet));
}

TEST_F(SceneExtensionRegistryTest, OrdersExtensionsOnceAndRetiresChildrenBeforeTheirScene)
{
    auto        a = std::make_shared<ExtensionProbe>("a", 20);
    auto        b = std::make_shared<ExtensionProbe>("b", 10);
    FramePacket packet;
    packet.featureCommands.push_back(RegisterSceneExtension{{1, 1}, {0, 1}, a});
    packet.featureCommands.push_back(RegisterSceneExtension{{2, 1}, {0, 1}, b});
    ASSERT_FALSE(registry.Validate(packet));
    registry.Apply(packet);
    auto borrowed = registry.SnapshotExtensions();
    ASSERT_EQ(borrowed.size(), 2u);
    EXPECT_EQ(borrowed[0].lease->registration.extension, b);
    FramePacket retire;
    retire.featureCommands.push_back(RetireRenderFeature{{0, 1}});
    ASSERT_FALSE(registry.Validate(retire));
    registry.Apply(retire);
    registry.CollectRetired();
    EXPECT_EQ(scene->retirements, 0u);
    borrowed.clear();
    registry.CollectRetired();
    EXPECT_EQ(a->retirements, 1u);
    EXPECT_EQ(b->retirements, 1u);
    EXPECT_EQ(scene->retirements, 1u);
}

TEST_F(SceneExtensionRegistryTest, RejectedProjectedRetirementDoesNotInvalidateBorrowers)
{
    FramePacket add;
    add.featureCommands.push_back(RegisterSceneExtension{{1, 1}, {0, 1}, std::make_shared<ExtensionProbe>("particle")});
    ASSERT_FALSE(registry.Validate(add));
    registry.Apply(add);
    const auto borrowed = registry.SnapshotExtensions();
    ASSERT_EQ(borrowed.size(), 1u);
    FramePacket invalid;
    invalid.featureCommands.push_back(RetireRenderFeature{{0, 1}});
    invalid.featureCommands.push_back(RegisterSceneExtension{{2, 1}, {0, 1}, std::make_shared<ExtensionProbe>("other")});
    EXPECT_TRUE(registry.Validate(invalid));
    EXPECT_TRUE(borrowed.front().CanBuild());
    EXPECT_EQ(registry.SnapshotExtensions().size(), 1u);
    EXPECT_TRUE(registry.CollectRetired().empty());
}

TEST_F(SceneExtensionRegistryTest, SceneReactivationDoesNotReviveOldExtensionSnapshots)
{
    FramePacket add;
    add.featureCommands.push_back(RegisterSceneExtension{{1, 1}, {0, 1}, std::make_shared<ExtensionProbe>("particle")});
    ASSERT_FALSE(registry.Validate(add));
    registry.Apply(add);
    const auto borrowed = registry.SnapshotExtensions();
    ASSERT_EQ(borrowed.size(), 1u);
    FramePacket disable;
    disable.featureCommands.push_back(SetRenderFeatureEnabled{{0, 1}, false});
    ASSERT_FALSE(registry.Validate(disable));
    registry.Apply(disable);
    EXPECT_FALSE(borrowed.front().CanBuild());
    EXPECT_TRUE(registry.SnapshotExtensions().empty());
    FramePacket enable;
    enable.featureCommands.push_back(SetRenderFeatureEnabled{{0, 1}, true});
    ASSERT_FALSE(registry.Validate(enable));
    registry.Apply(enable);
    EXPECT_FALSE(borrowed.front().CanBuild());
    const auto fresh = registry.SnapshotExtensions();
    ASSERT_EQ(fresh.size(), 1u);
    EXPECT_TRUE(fresh.front().CanBuild());
}
