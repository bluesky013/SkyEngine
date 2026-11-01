#include "OHOSInput.h"

#include <gtest/gtest.h>
#include <limits>

using namespace sky;

TEST(OHOSPointerTest, SecondaryContactsCannotMoveOrReleaseThePrimary)
{
    OHOSPointerState state;
    const auto       down = state.Touch(OHOSTouchAction::DOWN, 0, 10, 20);
    ASSERT_EQ(down.size(), 2U);
    EXPECT_EQ(down[0].type, OHOSMouseChange::Type::MOTION);
    EXPECT_EQ(down[0].relX, 0);
    EXPECT_EQ(down[1].type, OHOSMouseChange::Type::DOWN);
    EXPECT_TRUE(state.Touch(OHOSTouchAction::DOWN, 7, 200, 300).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::MOVE, 7, 250, 350).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::UP, 7, 250, 350).empty());
    const auto motion = state.Touch(OHOSTouchAction::MOVE, 0, 14, 27);
    ASSERT_EQ(motion.size(), 1U);
    EXPECT_EQ(motion[0].relX, 4);
    EXPECT_EQ(motion[0].relY, 7);
    const auto up = state.Touch(OHOSTouchAction::UP, 0, 14, 27);
    ASSERT_EQ(up.size(), 1U);
    EXPECT_EQ(up[0].type, OHOSMouseChange::Type::UP);
}

TEST(OHOSPointerTest, PrimaryReleaseCannotTurnRemainingContactsIntoAnotherPress)
{
    OHOSPointerState state;
    state.Touch(OHOSTouchAction::DOWN, 4, 10, 20);
    state.Touch(OHOSTouchAction::DOWN, 5, 100, 200);
    const auto released = state.Touch(OHOSTouchAction::UP, 4, 10, 20);
    ASSERT_EQ(released.size(), 1U);
    EXPECT_EQ(released[0].type, OHOSMouseChange::Type::UP);
    EXPECT_TRUE(state.Touch(OHOSTouchAction::MOVE, 5, 110, 210).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::DOWN, 6, 200, 300).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::UP, 5, 110, 210).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::UP, 6, 200, 300).empty());
    const auto restarted = state.Touch(OHOSTouchAction::DOWN, 8, 30, 40);
    ASSERT_EQ(restarted.size(), 2U);
    EXPECT_EQ(restarted[0].relX, 0);
    EXPECT_EQ(restarted[1].type, OHOSMouseChange::Type::DOWN);
}

TEST(OHOSPointerTest, DuplicateAndUnknownContactsDoNotForgeTransitions)
{
    OHOSPointerState state;
    EXPECT_TRUE(state.Touch(OHOSTouchAction::MOVE, 3, 10, 20).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::UP, 3, 10, 20).empty());
    state.Touch(OHOSTouchAction::DOWN, 3, 10, 20);
    EXPECT_TRUE(state.Touch(OHOSTouchAction::DOWN, 3, 30, 40).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::END, 3, 30, 40).empty());
    EXPECT_TRUE(state.Touch(static_cast<OHOSTouchAction>(-1), 3, 30, 40).empty());
    EXPECT_TRUE(state.Touch(static_cast<OHOSTouchAction>(100), 3, 30, 40).empty());
    const auto released = state.Cancel();
    ASSERT_EQ(released.size(), 1U);
    EXPECT_EQ(released[0].x, 10);
    EXPECT_EQ(released[0].y, 20);
}

TEST(OHOSPointerTest, CancellationReleasesEachButtonOnceAndClearsContacts)
{
    OHOSPointerState state;
    state.Mouse(10, 20, 6);
    state.Touch(OHOSTouchAction::DOWN, 1, 10, 20);
    state.Touch(OHOSTouchAction::DOWN, 2, 30, 40);
    const auto released = state.Cancel();
    ASSERT_EQ(released.size(), 3U);
    EXPECT_EQ(released[0].button, MouseButtonType::LEFT);
    EXPECT_EQ(released[1].button, MouseButtonType::RIGHT);
    EXPECT_EQ(released[2].button, MouseButtonType::MIDDLE);
    for (const auto &change : released) {
        EXPECT_EQ(change.type, OHOSMouseChange::Type::UP);
        EXPECT_EQ(change.x, 10);
        EXPECT_EQ(change.y, 20);
    }
    EXPECT_TRUE(state.Cancel().empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::MOVE, 1, 100, 200).empty());
    EXPECT_EQ(state.Touch(OHOSTouchAction::DOWN, 2, 30, 40).size(), 2U);
}

TEST(OHOSPointerTest, TouchAndMouseShareLogicalButtonsWithoutDuplicatePresses)
{
    OHOSPointerState state;
    state.Touch(OHOSTouchAction::DOWN, 1, 10, 20);
    EXPECT_TRUE(state.Mouse(10, 20, 1).empty());
    EXPECT_TRUE(state.Touch(OHOSTouchAction::UP, 1, 10, 20).empty());
    const auto released = state.Mouse(10, 20, 0);
    ASSERT_EQ(released.size(), 2U);
    EXPECT_EQ(released.back().type, OHOSMouseChange::Type::UP);
    EXPECT_EQ(released.back().button, MouseButtonType::LEFT);
}

TEST(OHOSPointerTest, TouchCancellationDoesNotReleasePhysicalMouseButtons)
{
    OHOSPointerState state;
    state.Mouse(10, 20, 2);
    state.Touch(OHOSTouchAction::DOWN, 1, 10, 20);
    const auto released = state.Touch(OHOSTouchAction::CANCEL, 1, 10, 20);
    ASSERT_EQ(released.size(), 1U);
    EXPECT_EQ(released[0].button, MouseButtonType::LEFT);
    const auto remaining = state.Cancel();
    ASSERT_EQ(remaining.size(), 1U);
    EXPECT_EQ(remaining[0].button, MouseButtonType::RIGHT);
}

TEST(OHOSPointerTest, MouseUsesButtonMaskAndPreservesFractionalWheelDelta)
{
    OHOSPointerState state;
    const auto       changes = state.Mouse(20, 30, 0xFF, 0.25f);
    ASSERT_EQ(changes.size(), 5U);
    EXPECT_EQ(changes[1].button, MouseButtonType::LEFT);
    EXPECT_EQ(changes[2].button, MouseButtonType::RIGHT);
    EXPECT_EQ(changes[3].button, MouseButtonType::MIDDLE);
    EXPECT_EQ(changes[4].type, OHOSMouseChange::Type::WHEEL);
    EXPECT_FLOAT_EQ(changes[4].scroll, 0.25f);
    EXPECT_TRUE(state.Mouse(20, 30, 7, std::numeric_limits<float>::quiet_NaN()).empty());
}

TEST(OHOSPointerTest, RelativeMotionCannotOverflowAtCoordinateLimits)
{
    OHOSPointerState state;
    state.Mouse(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), 0);
    const auto changes = state.Mouse(std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min(), 0);
    ASSERT_EQ(changes.size(), 1U);
    EXPECT_EQ(changes[0].relX, std::numeric_limits<int32_t>::max());
    EXPECT_EQ(changes[0].relY, std::numeric_limits<int32_t>::min());
}

TEST(OHOSKeyboardTest, RepeatsAreDeliveredButCancellationReleasesEachKeyOnce)
{
    OHOSKeyState state;
    EXPECT_TRUE(state.Apply(ScanCode::KEY_A, true, KeyMod::LEFT_SHIFT));
    EXPECT_TRUE(state.Apply(ScanCode::KEY_A, true, KeyMod::LEFT_SHIFT));
    EXPECT_TRUE(state.Apply(ScanCode::KEY_RETURN, true, {}));
    const auto released = state.Cancel();
    ASSERT_EQ(released.size(), 2U);
    EXPECT_EQ(released[0].scanCode, ScanCode::KEY_A);
    EXPECT_EQ(released[1].scanCode, ScanCode::KEY_RETURN);
    EXPECT_FALSE(released[0].mod);
    EXPECT_FALSE(released[1].mod);
    EXPECT_TRUE(state.Cancel().empty());
    EXPECT_FALSE(state.Apply(ScanCode::KEY_A, false, {}));
    EXPECT_TRUE(state.Apply(ScanCode::KEY_A, true, {}));
    EXPECT_TRUE(state.Apply(ScanCode::KEY_A, false, {}));
    EXPECT_FALSE(state.Apply(ScanCode::KEY_A, false, {}));
}

TEST(OHOSKeyboardTest, RejectsUnknownAndExclusiveBoundaryWithoutChangingPressedKeys)
{
    OHOSKeyState state;
    EXPECT_TRUE(state.Apply(ScanCode::KEY_KP_PERIOD, true, {}));
    EXPECT_FALSE(state.Apply(static_cast<ScanCode>(0), true, {}));
    EXPECT_FALSE(state.Apply(ScanCode::KEY_NUM, true, {}));
    EXPECT_FALSE(state.Apply(static_cast<ScanCode>(std::numeric_limits<uint32_t>::max()), true, {}));
    const auto released = state.Cancel();
    ASSERT_EQ(released.size(), 1U);
    EXPECT_EQ(released[0].scanCode, ScanCode::KEY_KP_PERIOD);
}

TEST(OHOSTextTest, PreeditNeverEmitsButIdenticalCommitDeltasRemainDistinct)
{
    const std::u16string text = u"\u4E2D";
    EXPECT_TRUE(OHOSCommittedText(text, true).empty());
    EXPECT_EQ(OHOSCommittedText(text), "\xE4\xB8\xAD");
    EXPECT_EQ(OHOSCommittedText(text), "\xE4\xB8\xAD");
    EXPECT_TRUE(OHOSCommittedText({}).empty());
}

TEST(OHOSTextTest, ConvertsSurrogatePairsAndReplacesUnpairedSurrogates)
{
    EXPECT_EQ(OHOSCommittedText(u"A\U0001F600\u4E2D"), "A\xF0\x9F\x98\x80\xE4\xB8\xAD");
    const std::u16string invalid = {0xD83D, u'A', 0xDE00, 0xD83D};
    EXPECT_EQ(OHOSCommittedText(invalid), "\xEF\xBF\xBD"
                                          "A"
                                          "\xEF\xBF\xBD\xEF\xBF\xBD");
}

TEST(OHOSTextTest, EmbeddedNulCannotTruncateOrJoinUnpairedSurrogates)
{
    const std::u16string embedded = {u'A', 0, u'B'};
    EXPECT_EQ(OHOSCommittedText(embedded), "AB");
    const std::u16string separated = {0xD83D, 0, 0xDE00};
    EXPECT_EQ(OHOSCommittedText(separated), "\xEF\xBF\xBD\xEF\xBF\xBD");
}
