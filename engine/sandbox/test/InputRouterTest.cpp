//
// Created on 2026/09/21.
//

#include <editor/core/input/InputRouter.h>
#include <gtest/gtest.h>

using namespace sky::editor;

TEST(InputRouterTest, DefaultDoesNotWantInput)
{
    InputRouter router;
    EXPECT_FALSE(router.WantsInput());
    EXPECT_FALSE(router.HasFocus());
    EXPECT_EQ(router.GetModalDepth(), 0u);
}

TEST(InputRouterTest, FocusWithoutConsumptionDoesNotGate)
{
    InputRouter router;
    router.SetFocus("viewport");
    EXPECT_TRUE(router.HasFocus());
    EXPECT_EQ(router.GetFocusPanel(), "viewport");
    // A focused panel that does not consume input does not gate the viewport.
    EXPECT_FALSE(router.WantsInput());
}

TEST(InputRouterTest, TextFieldFocusGatesInput)
{
    InputRouter router;
    router.SetFocus("console");
    router.SetFocusConsumesInput(true);
    EXPECT_TRUE(router.WantsInput());
}

TEST(InputRouterTest, ChangingFocusResetsConsumption)
{
    InputRouter router;
    router.SetFocus("console");
    router.SetFocusConsumesInput(true);
    ASSERT_TRUE(router.WantsInput());

    router.SetFocus("outliner");
    EXPECT_FALSE(router.GetFocusConsumesInput());
    EXPECT_FALSE(router.WantsInput());
}

TEST(InputRouterTest, ModalAlwaysWantsInput)
{
    InputRouter router;
    router.PushModal();
    EXPECT_TRUE(router.WantsInput());
    router.PushModal();
    EXPECT_EQ(router.GetModalDepth(), 2u);
    router.PopModal();
    EXPECT_TRUE(router.WantsInput());
    router.PopModal();
    EXPECT_FALSE(router.WantsInput());
    // Popping below zero is clamped.
    router.PopModal();
    EXPECT_EQ(router.GetModalDepth(), 0u);
}
