//
// Created on 2026/10/17.
//

#include <aurora/adaptor/renderer/RenderSceneProxy.h>
#include <aurora/adaptor/renderer/RendererHost.h>
#include <framework/interface/Interface.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {
    struct HostTrace {
        uint32_t creates = 0, polls = 0, stops = 0, destroys = 0;
    };
    class HostRenderer final : public IRenderer {
    public:
        explicit HostRenderer(HostTrace &trace) : trace(trace)
        {
        }
        ~HostRenderer() override
        {
            EXPECT_EQ(state, RendererState::STOPPED);
            ++trace.destroys;
        }
        PacketSubmitResult TrySubmit(std::unique_ptr<FramePacket> &) override
        {
            return {};
        }
        size_t PollEvents(std::span<RendererEvent> events) override
        {
            if (!events.empty() && state == RendererState::STOPPING && ++trace.polls == 2) {
                state     = RendererState::STOPPED;
                events[0] = RendererStopped{};
                return 1;
            }
            return 0;
        }
        RendererState GetState() const override
        {
            return state;
        }
        RendererInfo GetInfo() const override
        {
            return {API::VULKAN, 1};
        }
        void RequestShutdown() override
        {
            ++trace.stops;
            state = RendererState::STOPPING;
        }
        HostTrace    &trace;
        RendererState state = RendererState::RUNNING;
    };
    class HostProvider final : public IRendererProvider {
    public:
        RendererCreateResult Create(const RendererInitInfo &) override
        {
            ++trace.creates;
            if (fail)
                return RendererError{RendererErrorCode::OUT_OF_MEMORY, "injected creation failure"};
            return std::unique_ptr<IRenderer>(std::make_unique<HostRenderer>(trace));
        }
        HostTrace trace;
        bool      fail = false;
    };
} // namespace

TEST(RendererHost, PublishesOneOwnerAndDrainsBeforeUnregistering)
{
    HostProvider provider;
    RendererHost owner, duplicate;
    ASSERT_FALSE(owner.Init(provider, {}));
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), owner.Get());
    EXPECT_TRUE(duplicate.Init(provider, {}));
    EXPECT_EQ(provider.trace.creates, 1u);
    duplicate.Shutdown();
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), owner.Get());
    owner.Shutdown();
    EXPECT_EQ(provider.trace.polls, 2u);
    EXPECT_EQ(provider.trace.stops, 1u);
    EXPECT_EQ(provider.trace.destroys, 1u);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
    owner.Shutdown();
    EXPECT_EQ(provider.trace.stops, 1u);
}

TEST(RendererHost, FailedCreationLeavesNoPublishedOrPartiallyOwnedRenderer)
{
    HostProvider provider;
    provider.fail = true;
    RendererHost host;
    const auto   error = host.Init(provider, {});
    ASSERT_TRUE(error);
    EXPECT_EQ(error->code, RendererErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(host.Get(), nullptr);
    EXPECT_EQ(Interface<IRenderer>::Get()->GetApi(), nullptr);
    host.Shutdown();
    EXPECT_EQ(provider.trace.stops, 0u);
    provider.fail = false;
    EXPECT_FALSE(host.Init(provider, {}));
}

TEST(RenderSceneProxy, RetirementAcknowledgementGuardsReuseAndDoesNotBorrowSceneStorage)
{
    RenderSceneProxy scene;
    FramePacket      create, destroy, next;
    EXPECT_TRUE(scene.Create(create, {}));
    EXPECT_TRUE(create.commands.empty());
    ASSERT_FALSE(scene.Create(create, {4, 1}, {Name("main")}));
    EXPECT_EQ(scene.GetSceneId(), (SceneId{4, 1}));
    EXPECT_TRUE(scene.Create(next, {5, 1}));
    ASSERT_FALSE(scene.Destroy(destroy));
    EXPECT_TRUE(scene.IsRetiring());
    EXPECT_FALSE(scene.GetSceneId().IsValid());
    EXPECT_TRUE(scene.Destroy(destroy));
    EXPECT_EQ(destroy.commands.size(), 1u);
    scene.OnEvent(SceneDestroyed{{4, 2}});
    EXPECT_TRUE(scene.IsRetiring());
    scene.OnEvent(SceneDestroyed{{4, 1}});
    EXPECT_FALSE(scene.IsRetiring());
    ASSERT_FALSE(scene.Create(next, {4, 2}));
    EXPECT_EQ(std::get<CreateScene>(create.commands.front()).id, (SceneId{4, 1}));
    EXPECT_EQ(std::get<DestroyScene>(destroy.commands.front()).id, (SceneId{4, 1}));
    scene.OnRendererStopped();
    EXPECT_FALSE(scene.GetSceneId().IsValid());
}
