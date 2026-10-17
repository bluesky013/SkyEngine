//
// Created on 2026/10/15.
//

#include "AndroidInput.h"
#include <gtest/gtest.h>

using namespace sky;

TEST(AndroidLifecycleTest, SurfaceAndResumeAreBothRequiredForFrames)
{
    AndroidLifecycleState state;
    EXPECT_FALSE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::SURFACE_AVAILABLE);
    EXPECT_FALSE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::RESUME);
    EXPECT_TRUE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::PAUSE);
    EXPECT_FALSE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::RESUME);
    EXPECT_TRUE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::SURFACE_LOST);
    EXPECT_FALSE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::SURFACE_AVAILABLE);
    EXPECT_TRUE(state.IsFrameActive());
    state.Apply(AndroidLifecycleEvent::DESTROY);
    state.Apply(AndroidLifecycleEvent::RESUME);
    state.Apply(AndroidLifecycleEvent::SURFACE_AVAILABLE);
    EXPECT_FALSE(state.IsFrameActive());
    EXPECT_TRUE(state.IsDestroyed());
}

TEST(AndroidLifecycleTest, FocusEventsRemainIndependentOfSurfaceRecreation)
{
    AndroidLifecycleState state;
    state.Apply(AndroidLifecycleEvent::FOCUS_GAINED);
    state.Apply(AndroidLifecycleEvent::SURFACE_LOST);
    EXPECT_TRUE(state.HasFocus());
    state.Apply(AndroidLifecycleEvent::FOCUS_LOST);
    EXPECT_FALSE(state.HasFocus());
    state.Apply(AndroidLifecycleEvent::DESTROY);
    state.Apply(AndroidLifecycleEvent::FOCUS_GAINED);
    EXPECT_FALSE(state.HasFocus());
}

TEST(AndroidLifecycleTest, EpochRecordsSuspendResumeEvenWithoutAnInactiveFrame)
{
    AndroidLifecycleState state;
    state.Apply(AndroidLifecycleEvent::RESUME);
    state.Apply(AndroidLifecycleEvent::SURFACE_AVAILABLE);
    const uint64_t activeEpoch = state.GetEpoch();
    state.Apply(AndroidLifecycleEvent::RESUME);
    state.Apply(AndroidLifecycleEvent::SURFACE_AVAILABLE);
    EXPECT_EQ(state.GetEpoch(), activeEpoch);
    state.Apply(AndroidLifecycleEvent::PAUSE);
    state.Apply(AndroidLifecycleEvent::RESUME);
    EXPECT_TRUE(state.IsFrameActive());
    EXPECT_GT(state.GetEpoch(), activeEpoch);
    const uint64_t resumedEpoch = state.GetEpoch();
    state.Apply(AndroidLifecycleEvent::SURFACE_LOST);
    state.Apply(AndroidLifecycleEvent::SURFACE_AVAILABLE);
    EXPECT_GT(state.GetEpoch(), resumedEpoch);
}

TEST(AndroidPointerTest, TracksPointerIdAcrossIndexChangesAndIgnoresSecondFinger)
{
    AndroidPointerState   state;
    AndroidMotionSnapshot motion;
    motion.action   = AndroidMotionAction::DOWN;
    motion.pointers = {{12, 10, 20}};
    auto changes    = state.Apply(motion);
    ASSERT_EQ(changes.size(), 2U);
    EXPECT_EQ(changes.back().type, AndroidMouseChange::Type::DOWN);
    motion.action      = AndroidMotionAction::POINTER_DOWN;
    motion.actionIndex = 1;
    motion.pointers.push_back({99, 200, 300});
    EXPECT_TRUE(state.Apply(motion).empty());
    motion.action      = AndroidMotionAction::MOVE;
    motion.actionIndex = 0;
    motion.pointers    = {{99, 200, 300}, {12, 14, 27}};
    changes            = state.Apply(motion);
    ASSERT_EQ(changes.size(), 1U);
    EXPECT_EQ(changes[0].x, 14);
    EXPECT_EQ(changes[0].relX, 4);
    EXPECT_EQ(changes[0].relY, 7);
    motion.action = AndroidMotionAction::POINTER_UP;
    EXPECT_TRUE(state.Apply(motion).empty());
    motion.actionIndex = 1;
    changes            = state.Apply(motion);
    ASSERT_EQ(changes.size(), 1U);
    EXPECT_EQ(changes[0].type, AndroidMouseChange::Type::UP);
    motion.action      = AndroidMotionAction::MOVE;
    motion.actionIndex = 0;
    motion.pointers    = {{99, 201, 301}};
    EXPECT_TRUE(state.Apply(motion).empty());
}

TEST(AndroidPointerTest, CancellationReleasesPressedButtonsOnce)
{
    AndroidPointerState   state;
    AndroidMotionSnapshot motion;
    motion.action      = AndroidMotionAction::BUTTON;
    motion.touch       = false;
    motion.buttons     = 3;
    motion.pointers    = {{0, 10, 20}};
    const auto changes = state.Apply(motion);
    ASSERT_EQ(changes.size(), 3U);
    const auto released = state.Cancel();
    ASSERT_EQ(released.size(), 2U);
    EXPECT_EQ(released[0].type, AndroidMouseChange::Type::UP);
    EXPECT_EQ(released[0].button, MouseButtonType::LEFT);
    EXPECT_EQ(released[1].button, MouseButtonType::RIGHT);
    EXPECT_TRUE(state.Cancel().empty());
}

TEST(AndroidPointerTest, FractionalWheelDeltaAndInvalidPointersAreHandled)
{
    AndroidPointerState   state;
    AndroidMotionSnapshot motion;
    motion.action      = AndroidMotionAction::SCROLL;
    motion.touch       = false;
    motion.pointers    = {{0, 10, 20}};
    motion.scroll      = 0.25f;
    const auto changes = state.Apply(motion);
    ASSERT_EQ(changes.size(), 2U);
    EXPECT_EQ(changes.back().type, AndroidMouseChange::Type::WHEEL);
    EXPECT_FLOAT_EQ(changes.back().scroll, 0.25f);
    motion.actionIndex = 4;
    EXPECT_TRUE(state.Apply(motion).empty());
}

TEST(AndroidKeyboardTest, UsesAndroidKeyCodesAndPreservesModifierSides)
{
    EXPECT_EQ(AndroidKeyCodeToScanCode(29), ScanCode::KEY_A);
    EXPECT_EQ(AndroidKeyCodeToScanCode(54), ScanCode::KEY_Z);
    EXPECT_EQ(AndroidKeyCodeToScanCode(7), ScanCode::KEY_0);
    EXPECT_EQ(AndroidKeyCodeToScanCode(111), ScanCode::KEY_ESCAPE);
    EXPECT_EQ(AndroidKeyCodeToScanCode(131), ScanCode::KEY_F1);
    EXPECT_EQ(AndroidKeyCodeToScanCode(153), ScanCode::KEY_KP_9);
    EXPECT_EQ(AndroidKeyCodeToScanCode(0), ScanCode::KEY_NUM);
    const auto mods = AndroidMetaStateToModifiers(0x80 | 0x2000 | 0x20 | 0x40000 | 0x100000 | 0x200000);
    EXPECT_TRUE(mods.TestBit(KeyMod::RIGHT_SHIFT));
    EXPECT_FALSE(mods.TestBit(KeyMod::LEFT_SHIFT));
    EXPECT_TRUE(mods.TestBit(KeyMod::LEFT_CTRL));
    EXPECT_TRUE(mods.TestBit(KeyMod::RIGHT_ALT));
    EXPECT_TRUE(mods.TestBit(KeyMod::RIGHT_GUI));
    EXPECT_TRUE(mods.TestBit(KeyMod::CAPS));
    EXPECT_TRUE(mods.TestBit(KeyMod::NUM));
    EXPECT_TRUE(AndroidMetaStateToModifiers(1).TestBit(KeyMod::LEFT_SHIFT));
}

TEST(AndroidTextTest, EmitsCommittedTextOnceAndPreservesUtf8Boundaries)
{
    AndroidTextState state;
    EXPECT_EQ(state.Apply("a", false).inserted, "a");
    EXPECT_TRUE(state.Apply("a", false).inserted.empty());
    EXPECT_TRUE(state.Apply("a\xE4\xB8\xAD", true).inserted.empty());
    EXPECT_EQ(state.Apply("a\xE4\xB8\xAD", false).inserted, "\xE4\xB8\xAD");
    const auto replacement = state.Apply("a\xE4\xB8\xAE", false);
    EXPECT_EQ(replacement.inserted, "\xE4\xB8\xAE");
    EXPECT_EQ(replacement.deleted, 1U);
    const auto deletion = state.Apply("a", false);
    EXPECT_TRUE(deletion.inserted.empty());
    EXPECT_EQ(deletion.deleted, 1U);
}

TEST(AndroidTextTest, ConvertsModifiedUtf8SurrogatePairsAndResetsBetweenFields)
{
    AndroidTextState state;
    EXPECT_EQ(state.Apply("\xED\xA0\xBD\xED\xB8\x80", false).inserted, "\xF0\x9F\x98\x80");
    EXPECT_TRUE(state.Apply("\xED\xA0\xBD\xED\xB8\x80", false).inserted.empty());
    state.Reset();
    EXPECT_EQ(state.Apply("x", false).inserted, "x");
    EXPECT_EQ(state.Apply("\xED\xA0\xBD", false).inserted, "\xEF\xBF\xBD");
}

TEST(AndroidTextTest, AdjacentCjkEditsDoNotSplitSharedUtf8PrefixOrSuffix)
{
    AndroidTextState state;
    state.Apply("a\xE4\xB8\xAD"
                "x",
                false);
    const auto replacement = state.Apply("a\xE4\xB8\xAE"
                                         "x",
                                         false);
    EXPECT_EQ(replacement.deleted, 1U);
    EXPECT_EQ(replacement.inserted, "\xE4\xB8\xAE");
    state.Reset();
    state.Apply("\xE4\xB8\x8A", false);
    const auto sharedLastByte = state.Apply("\xE9\xBE\x8A", false);
    EXPECT_EQ(sharedLastByte.deleted, 1U);
    EXPECT_EQ(sharedLastByte.inserted, "\xE9\xBE\x8A");
    state.Reset();
    state.Apply("\xF0\x9F\x98\x80", false);
    const auto differentLengths = state.Apply("\xE4\xB8\x80", false);
    EXPECT_EQ(differentLengths.deleted, 1U);
    EXPECT_EQ(differentLengths.inserted, "\xE4\xB8\x80");
    const auto insertionBefore = state.Apply("a\xE4\xB8\x80", false);
    EXPECT_EQ(insertionBefore.deleted, 0U);
    EXPECT_EQ(insertionBefore.inserted, "a");
}
