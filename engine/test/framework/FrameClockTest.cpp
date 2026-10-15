#include <framework/application/FrameClock.h>
#include <gtest/gtest.h>

#include <limits>

TEST(FrameClockTest, RealFrameRateAfterInitialSample)
{
    sky::FrameClock clock;
    EXPECT_FLOAT_EQ(clock.Sample(0, 120), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(clock.Sample(1, 120), 1.0f / 120.0f);
    EXPECT_FLOAT_EQ(clock.Sample(5, 120), 1.0f / 30.0f);
}

TEST(FrameClockTest, ResumeDoesNotIncludeBackgroundTime)
{
    sky::FrameClock clock;
    clock.Sample(100, 1000);
    clock.Sample(116, 1000);
    clock.Reset();
    EXPECT_FLOAT_EQ(clock.Sample(600116, 1000), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(clock.Sample(600136, 1000), 0.02f);
}

TEST(FrameClockTest, ApplicationsHaveIndependentSamples)
{
    sky::FrameClock first;
    sky::FrameClock second;
    first.Sample(1000, 1000);
    EXPECT_FLOAT_EQ(second.Sample(2000, 1000), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(first.Sample(1020, 1000), 0.02f);
}

TEST(FrameClockTest, ExternalLoopResumeWithoutAnInactiveFrameDiscardsBackgroundTime)
{
    sky::FrameClock clock;
    clock.Sample(100, 1000, 0);
    EXPECT_FLOAT_EQ(clock.Sample(116, 1000, 0), 0.016f);
    // A stopped CADisplayLink produces no call to Sample or Reset while paused.
    EXPECT_FLOAT_EQ(clock.Sample(600116, 1000, 2), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(clock.Sample(600136, 1000, 2), 0.02f);
}

TEST(FrameClockTest, InvalidOrChangedClockRestartsSampling)
{
    sky::FrameClock clock;
    clock.Sample(100, 1000);
    EXPECT_FLOAT_EQ(clock.Sample(120, 0), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(clock.Sample(140, 1000), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(clock.Sample(120, 1000), 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(clock.Sample(150, 2000), 1.0f / 60.0f);
}

TEST(FrameClockTest, LargeCountersRetainSubFramePrecision)
{
    sky::FrameClock clock;
    constexpr auto  base = std::numeric_limits<uint64_t>::max() - 100;
    clock.Sample(base, 1000000000);
    EXPECT_FLOAT_EQ(clock.Sample(base + 8, 1000000000), 8.0e-9f);
    EXPECT_FLOAT_EQ(clock.Sample(base + 8, 1000000000), 0.0f);
}
