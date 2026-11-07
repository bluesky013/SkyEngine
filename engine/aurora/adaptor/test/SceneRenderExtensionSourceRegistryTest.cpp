#include <aurora/adaptor/renderer/SceneRenderExtensionSourceRegistry.h>
#include <gtest/gtest.h>

namespace {
    using namespace sky;
    using namespace sky::aurora;

    struct Recorder {
        std::vector<int> calls;
    };

    class TestSource final : public ISceneRenderExtensionSource {
    public:
        TestSource(Recorder &recorder, int id) : recorder(recorder), id(id)
        {
        }

        std::optional<RendererError> Append(FramePacket &, const SceneExtensionSourceContext &) override
        {
            recorder.calls.push_back(id);
            if (fail)
                return RendererError{RendererErrorCode::INVALID_PACKET, "test source failure"};
            return {};
        }

        Recorder &recorder;
        int       id;
        bool      fail = false;
    };
} // namespace

TEST(SceneRenderExtensionSourceRegistry, AppendUsesRegistrationOrderAndTracksIdentity)
{
    auto        *registry = SceneRenderExtensionSourceRegistry::Get();
    Recorder     recorder;
    TestSource   first(recorder, 1);
    TestSource   second(recorder, 2);
    FramePacket  packet;
    const SceneExtensionSourceContext context{};

    ASSERT_TRUE(registry->Register(first));
    ASSERT_FALSE(registry->Register(first));
    ASSERT_TRUE(registry->Register(second));
    EXPECT_FALSE(registry->Empty());

    EXPECT_FALSE(registry->Append(packet, context).has_value());
    EXPECT_EQ(recorder.calls, (std::vector<int>{1, 2}));

    recorder.calls.clear();
    registry->UnRegister(first);
    EXPECT_FALSE(registry->Append(packet, context).has_value());
    EXPECT_EQ(recorder.calls, (std::vector<int>{2}));

    registry->UnRegister(second);
    EXPECT_TRUE(registry->Empty());
}

TEST(SceneRenderExtensionSourceRegistry, AppendPropagatesFirstError)
{
    auto        *registry = SceneRenderExtensionSourceRegistry::Get();
    Recorder     recorder;
    TestSource   failing(recorder, 1);
    TestSource   skipped(recorder, 2);
    failing.fail = true;
    FramePacket packet;
    const SceneExtensionSourceContext context{};

    ASSERT_TRUE(registry->Register(failing));
    ASSERT_TRUE(registry->Register(skipped));

    const auto result = registry->Append(packet, context);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->code, RendererErrorCode::INVALID_PACKET);
    EXPECT_EQ(recorder.calls, (std::vector<int>{1}));

    registry->UnRegister(failing);
    registry->UnRegister(skipped);
}
