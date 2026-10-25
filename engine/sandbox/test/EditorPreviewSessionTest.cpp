#include "EditorPreviewSessions.h"

#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::editor;
using namespace sky::aurora;

namespace {
    struct EmptySource : ISceneResourceSource {
        SceneResourceResult ResolveMesh(const Uuid &, const Uuid &, RendererInfo) override
        {
            return {};
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
    struct PreviewSessions : testing::Test {
        uint32_t              next = 10;
        UiSnapshotProducer    textures{1};
        EditorPreviewSessions host{{API::VULKAN, 1}, std::make_shared<EmptySource>(), textures, [this] { return ViewportId{next++, 1}; }};
        AssetPreviewHandle    Create(std::shared_ptr<const void> lease = std::make_shared<int>(1))
        {
            AssetPreviewScene scene;
            scene.moduleLease = std::move(lease);
            scene.world       = World::CreateWorld();
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
    const auto scenes = Commands<CreateScene>(create);
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
TEST_F(PreviewSessions, CloseRetainsWorldCodeUntilEveryRendererObjectRetires)
{
    auto                      lease    = std::make_shared<int>(1);
    std::weak_ptr<const void> lifetime = lease;
    const auto                handle   = Create(lease);
    lease.reset();
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
    EXPECT_FALSE(lifetime.expired());
    host.OnEvent(SceneDestroyed{scene});
    host.OnEvent(ViewDestroyed{view});
    EXPECT_FALSE(lifetime.expired());
    host.OnEvent(ViewportDestroyed{output});
    EXPECT_TRUE(lifetime.expired());
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(handle)).state, AssetPreviewState::Closed);
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.Update(handle, {})));
}
TEST_F(PreviewSessions, UnsubmittedCloseAndStoppedHostReleaseSessionsWithoutGpuClaims)
{
    auto                      lease    = std::make_shared<int>(1);
    std::weak_ptr<const void> lifetime = lease;
    const auto                handle   = Create(lease);
    lease.reset();
    host.CloseSession(handle);
    FramePacket packet;
    packet.sequence = 1;
    ASSERT_FALSE(host.Append(packet));
    EXPECT_TRUE(packet.commands.empty());
    EXPECT_TRUE(lifetime.expired());
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(handle)).state, AssetPreviewState::Closed);
    const auto second = Create();
    host.OnRendererStopped();
    EXPECT_EQ(std::get<AssetPreviewStatus>(host.Status(second)).state, AssetPreviewState::Closed);
    AssetPreviewScene scene;
    EXPECT_TRUE(std::holds_alternative<AssetPreviewError>(host.CreateSession(std::move(scene))));
}
