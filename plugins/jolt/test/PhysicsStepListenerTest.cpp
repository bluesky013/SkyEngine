#include "JoltBackend.h"
#include <gtest/gtest.h>
#include <jolt/PhysicsSystem.h>
#include <physics/PhysicsBackendRegistry.h>

#include <functional>

using namespace sky;
using namespace sky::phy;

namespace {
    struct StepRecorder final : IPhysicsStepListener {
        std::vector<uint64_t> before;
        std::vector<uint64_t> after;
        uint32_t              closing    = 0;
        bool                  failBefore = false;
        bool                  failAfter  = false;
        std::function<void()> duringBefore;
        std::function<void()> duringClosing;

        PhysicsStatus BeforePhysicsStep(IPhysicsWorld &world, float delta) override
        {
            EXPECT_GT(delta, 0.f);
            before.push_back(world.GetStats().stepCount);
            if (duringBefore) {
                duringBefore();
            }
            return failBefore ? PhysicsStatus{PhysicsError::InvalidState, "Input failed"} : PhysicsStatus{};
        }
        PhysicsStatus AfterPhysicsStep(IPhysicsWorld &world, float delta) override
        {
            after.push_back(world.GetStats().stepCount);
            return failAfter ? PhysicsStatus{PhysicsError::InvalidState, "Publication failed"} : PhysicsStatus{};
        }
        void OnPhysicsWorldClosing(IPhysicsWorld &world) override
        {
            ++closing;
            if (duringClosing) {
                duringClosing();
            }
        }
    };

    class PhysicsStepListenerTest : public testing::Test {
        void SetUp() override
        {
            ASSERT_TRUE(PhysicsBackendRegistry::Get().Register(std::make_unique<JoltBackend>()));
        }
        void TearDown() override
        {
            PhysicsBackendRegistry::Get().Unregister();
        }
    };
} // namespace

TEST_F(PhysicsStepListenerTest, PausedTickAndSingleStepHaveExactBoundaries)
{
    const auto recorder = std::make_shared<StepRecorder>();
    {
        PhysicsSystem system;
        ASSERT_TRUE(system.Init());
        ASSERT_TRUE(system.AddStepListener(recorder));
        EXPECT_FALSE(system.AddStepListener(recorder));
        system.SetSimulating(false);
        system.Tick(1.f / 30.f);
        EXPECT_TRUE(recorder->before.empty());
        system.SingleStep();
        EXPECT_EQ(recorder->before, (std::vector<uint64_t>{0}));
        EXPECT_EQ(recorder->after, (std::vector<uint64_t>{1}));
        EXPECT_FLOAT_EQ(system.GetInterpolationAlpha(), 1.f);
        system.SetSimulating(true);
        system.Tick(1.f / 30.f);
        EXPECT_EQ(recorder->before, (std::vector<uint64_t>{0, 1, 2}));
        EXPECT_EQ(recorder->after, (std::vector<uint64_t>{1, 2, 3}));
    }
    EXPECT_EQ(recorder->closing, 1u);
}

TEST_F(PhysicsStepListenerTest, InputFailureStopsBeforeNativeStepAndCanRetry)
{
    PhysicsSystem system;
    ASSERT_TRUE(system.Init());
    const auto recorder  = std::make_shared<StepRecorder>();
    recorder->failBefore = true;
    ASSERT_TRUE(system.AddStepListener(recorder));
    system.SingleStep();
    EXPECT_FALSE(system.GetLastStatus());
    EXPECT_FALSE(system.IsSimulating());
    EXPECT_EQ(system.GetStats().stepCount, 0u);
    EXPECT_TRUE(recorder->after.empty());
    recorder->failBefore = false;
    system.SingleStep();
    EXPECT_TRUE(system.GetLastStatus());
    EXPECT_EQ(system.GetStats().stepCount, 1u);
}

TEST_F(PhysicsStepListenerTest, PublicationFailureStillNotifiesRemainingListeners)
{
    PhysicsSystem system;
    ASSERT_TRUE(system.Init());
    const auto first  = std::make_shared<StepRecorder>();
    const auto second = std::make_shared<StepRecorder>();
    first->failAfter  = true;
    ASSERT_TRUE(system.AddStepListener(first));
    ASSERT_TRUE(system.AddStepListener(second));
    system.Tick(1.f / 30.f);
    EXPECT_FALSE(system.GetLastStatus());
    EXPECT_EQ(system.GetStats().stepCount, 1u);
    EXPECT_EQ(second->after, (std::vector<uint64_t>{1}));
}

TEST_F(PhysicsStepListenerTest, RemovingListenerDuringCallbackIsSafe)
{
    PhysicsSystem system;
    ASSERT_TRUE(system.Init());
    const auto first    = std::make_shared<StepRecorder>();
    const auto second   = std::make_shared<StepRecorder>();
    first->duringBefore = [&] { system.RemoveStepListener(second.get()); };
    ASSERT_TRUE(system.AddStepListener(first));
    ASSERT_TRUE(system.AddStepListener(second));
    system.SingleStep();
    EXPECT_TRUE(second->before.empty());
    EXPECT_TRUE(second->after.empty());
    EXPECT_EQ(first->after.size(), 1u);
}

TEST_F(PhysicsStepListenerTest, ReentrantTickDoesNotConsumeAccumulator)
{
    PhysicsSystem system;
    ASSERT_TRUE(system.Init());
    const auto recorder    = std::make_shared<StepRecorder>();
    recorder->duringBefore = [&] { system.Tick(1.f / 120.f); };
    ASSERT_TRUE(system.AddStepListener(recorder));
    system.SingleStep();
    recorder->duringBefore = {};
    system.Tick(1.f / 120.f);
    EXPECT_EQ(system.GetStats().stepCount, 1u);
    system.Tick(1.f / 120.f);
    EXPECT_EQ(system.GetStats().stepCount, 2u);
}

TEST_F(PhysicsStepListenerTest, ClosingRejectsNewRegistrations)
{
    const auto recorder = std::make_shared<StepRecorder>();
    const auto late     = std::make_shared<StepRecorder>();
    {
        PhysicsSystem system;
        ASSERT_TRUE(system.Init());
        recorder->duringClosing = [&] { EXPECT_FALSE(system.AddStepListener(late)); };
        ASSERT_TRUE(system.AddStepListener(recorder));
    }
    recorder->duringClosing = {};
    EXPECT_EQ(recorder->closing, 1u);
    EXPECT_EQ(late->closing, 0u);
}
