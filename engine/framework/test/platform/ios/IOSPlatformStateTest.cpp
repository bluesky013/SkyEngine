//
// Portable regression coverage for UIKit's lifecycle and primary-touch policy.
//

#include "../../../../framework/platform/ios/IOSPlatformState.h"

#include <gtest/gtest.h>

using namespace sky::ios;

TEST(IOSPlatformStateTest, StartupAndForegroundRequireConnectedActiveSceneAndSurface)
{
    FrameState state;
    state.Start();
    ASSERT_TRUE(state.ConnectScene(1));
    state.SetSurfaceAvailable(true);
    EXPECT_FALSE(state.ShouldRunLoop()); // A connected scene is not active until its activation callback.
    state.SetSceneActive(1, true);
    EXPECT_TRUE(state.ShouldRunLoop());
    state.SetSceneActive(1, false);
    EXPECT_FALSE(state.CanTick());
    EXPECT_FALSE(state.ShouldRunLoop()); // willEnterForeground is still inactive.
    state.SetSceneActive(1, true);
    EXPECT_TRUE(state.ShouldRunLoop());
    state.SetSurfaceAvailable(false);
    EXPECT_FALSE(state.CanTick());
    EXPECT_FALSE(state.ShouldRunLoop());
}

TEST(IOSPlatformStateTest, StopIsPermanentAcrossLaterResumeNotifications)
{
    FrameState state;
    ASSERT_TRUE(state.ConnectScene(1));
    state.SetSceneActive(1, true);
    state.SetSurfaceAvailable(true);
    state.Start();
    state.Stop();
    state.SetSceneActive(1, false);
    state.SetSceneActive(1, true);
    EXPECT_FALSE(state.ShouldRunLoop());
    state.Start();
    EXPECT_TRUE(state.ShouldRunLoop());
}

TEST(IOSPlatformStateTest, LifecycleEpochChangesEvenWhenNoFramesRunDuringBackground)
{
    FrameState state;
    ASSERT_TRUE(state.ConnectScene(1));
    state.SetSurfaceAvailable(true);
    state.SetSceneActive(1, true);
    state.Start();
    const uint64_t lastFrameEpoch = state.GetEpoch();
    state.SetSceneActive(1, false);
    const uint64_t backgroundEpoch = state.GetEpoch();
    EXPECT_GT(backgroundEpoch, lastFrameEpoch);
    state.SetSceneActive(1, false);
    EXPECT_EQ(state.GetEpoch(), backgroundEpoch);
    // UIKit stops CADisplayLink: no application Loop observes the inactive state.
    state.SetSceneActive(1, true);
    EXPECT_GT(state.GetEpoch(), backgroundEpoch);
    EXPECT_NE(state.GetEpoch(), lastFrameEpoch);
    const uint64_t resumedEpoch = state.GetEpoch();
    state.SetSceneActive(1, true);
    state.SetSurfaceAvailable(true);
    EXPECT_EQ(state.GetEpoch(), resumedEpoch);
    state.SetSurfaceAvailable(false);
    EXPECT_GT(state.GetEpoch(), resumedEpoch);
}

TEST(IOSPlatformStateTest, NullAndForeignScenesCannotChangeActiveIdentityOrEpoch)
{
    FrameState state;
    EXPECT_FALSE(state.ConnectScene(0));
    ASSERT_TRUE(state.ConnectScene(1));
    state.SetSurfaceAvailable(true);
    state.SetSceneActive(1, true);
    state.Start();
    const uint64_t epoch = state.GetEpoch();
    EXPECT_FALSE(state.ConnectScene(2));
    EXPECT_FALSE(state.SetSceneActive(0, false));
    EXPECT_FALSE(state.SetSceneActive(2, false));
    EXPECT_FALSE(state.DisconnectScene(2));
    EXPECT_EQ(state.GetSceneIdentity(), 1U);
    EXPECT_EQ(state.GetEpoch(), epoch);
    EXPECT_TRUE(state.ShouldRunLoop());
    EXPECT_TRUE(state.ConnectScene(1));
    EXPECT_EQ(state.GetEpoch(), epoch);
}

TEST(IOSPlatformStateTest, SceneDisconnectClearsSurfaceAndActiveStateWhilePreservingFrameRequest)
{
    FrameState state;
    ASSERT_TRUE(state.ConnectScene(1));
    state.SetSurfaceAvailable(true);
    state.SetSceneActive(1, true);
    state.Start();
    const uint64_t activeEpoch = state.GetEpoch();
    ASSERT_TRUE(state.DisconnectScene(1));
    EXPECT_EQ(state.GetSceneIdentity(), 0U);
    EXPECT_FALSE(state.CanTick());
    EXPECT_FALSE(state.ShouldRunLoop());
    EXPECT_GT(state.GetEpoch(), activeEpoch);
    const uint64_t detachedEpoch = state.GetEpoch();
    EXPECT_FALSE(state.DisconnectScene(1));
    EXPECT_FALSE(state.SetSceneActive(1, true));
    EXPECT_EQ(state.GetEpoch(), detachedEpoch);
    ASSERT_TRUE(state.ConnectScene(2));
    EXPECT_EQ(state.GetSceneIdentity(), 2U);
    EXPECT_GT(state.GetEpoch(), detachedEpoch);
    state.SetSurfaceAvailable(true);
    EXPECT_FALSE(state.ShouldRunLoop());
    state.SetSceneActive(1, true);
    EXPECT_FALSE(state.ShouldRunLoop());
    state.SetSceneActive(2, true);
    EXPECT_TRUE(state.ShouldRunLoop());
}

TEST(IOSPlatformStateTest, ExplicitStopIsNotRevivedByASceneReplacement)
{
    FrameState state;
    ASSERT_TRUE(state.ConnectScene(1));
    state.SetSurfaceAvailable(true);
    state.SetSceneActive(1, true);
    state.Start();
    state.Stop();
    ASSERT_TRUE(state.DisconnectScene(1));
    ASSERT_TRUE(state.ConnectScene(2));
    state.SetSurfaceAvailable(true);
    state.SetSceneActive(2, true);
    EXPECT_TRUE(state.CanTick());
    EXPECT_FALSE(state.ShouldRunLoop());
    state.Start();
    EXPECT_TRUE(state.ShouldRunLoop());
}

TEST(IOSPlatformStateTest, TouchResizeAndInputRectUseTheSameFramebufferScale)
{
    EXPECT_EQ(ToFramebufferCoordinate(10.5, 3.0), 32);
    EXPECT_EQ(ToFramebufferCoordinate(-2.0, 2.0), -4);
    EXPECT_EQ(ToFramebufferExtent(390.0, 3.0), 1170U);
    EXPECT_EQ(ToFramebufferExtent(0.0, 3.0), 0U);
    EXPECT_DOUBLE_EQ(ValidScale(0.0), 1.0);
    EXPECT_EQ(ToFramebufferCoordinate(10.0, 0.0), 10);
}

TEST(IOSPlatformStateTest, SecondaryFingerCannotMoveOrReleasePrimaryDrag)
{
    PrimaryTouch touch;
    ASSERT_TRUE(touch.Begin(1, {30, 60}));
    EXPECT_FALSE(touch.Begin(2, {300, 600}));
    PixelPoint delta;
    EXPECT_FALSE(touch.Move(2, {400, 700}, delta));
    EXPECT_FALSE(touch.End(2, {400, 700}));
    ASSERT_TRUE(touch.Move(1, {36, 54}, delta));
    EXPECT_EQ(delta.x, 6);
    EXPECT_EQ(delta.y, -6);
    EXPECT_TRUE(touch.End(1, {40, 50}));
    EXPECT_FALSE(touch.Move(2, {400, 700}, delta));
    EXPECT_FALSE(touch.Cancel());
}

TEST(IOSPlatformStateTest, PauseOrCancellationReleasesPrimaryOnceAtItsLastPosition)
{
    PrimaryTouch touch;
    ASSERT_TRUE(touch.Begin(1, {30, 60}));
    EXPECT_TRUE(touch.Cancel());
    EXPECT_FALSE(touch.Cancel());
    EXPECT_EQ(touch.GetPosition().x, 30);
    EXPECT_EQ(touch.GetPosition().y, 60);
    EXPECT_FALSE(touch.End(1, {30, 60}));
    EXPECT_TRUE(touch.Begin(2, {90, 120}));
}
