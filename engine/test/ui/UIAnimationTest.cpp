//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/animation/UIAnimation.h>

#include <memory>

namespace sky::ui {

    TEST(UIAnimationTest, EasingValues)
    {
        EXPECT_FLOAT_EQ(ApplyEasing(UIEasing::LINEAR, 0.5f), 0.5f);
        EXPECT_FLOAT_EQ(ApplyEasing(UIEasing::EASE_OUT_QUAD, 0.5f), 0.75f);
        EXPECT_FLOAT_EQ(ApplyEasing(UIEasing::EASE_IN_QUAD, 0.5f), 0.25f);
    }

    TEST(UIAnimationTest, TrackInterpolates)
    {
        UIFloatTrack track;
        track.AddKey(1.0f, 10.0f);
        track.AddKey(0.0f, 0.0f);

        EXPECT_FLOAT_EQ(track.Evaluate(0.5f), 5.0f);
        EXPECT_FLOAT_EQ(track.Evaluate(-1.0f), 0.0f);
        EXPECT_FLOAT_EQ(track.Evaluate(2.0f), 10.0f);
    }

    TEST(UIAnimationTest, TweenUpdatesProgress)
    {
        float last = -1.0f;
        UIAnimation animation(1.0f, [&last](float value) { last = value; });

        animation.Advance(0.5f);
        EXPECT_FLOAT_EQ(last, 0.5f);
        EXPECT_TRUE(animation.IsComplete() == false);
    }

    TEST(UIAnimationTest, ContextRemovesCompleted)
    {
        UIContext context;
        int completed = 0;

        auto animation = std::make_unique<UIAnimation>(1.0f, [](float) {});
        animation->SetOnComplete([&completed]() { completed++; });
        context.AddAnimation(std::move(animation));
        EXPECT_EQ(context.GetAnimationCount(), 1u);

        context.Tick(2.0f);
        EXPECT_EQ(completed, 1);
        EXPECT_EQ(context.GetAnimationCount(), 0u);
    }

    TEST(UIAnimationTest, LoopWrapsAndStaysActive)
    {
        UIContext context;
        auto *animation = context.AddAnimation(std::make_unique<UIAnimation>(1.0f, [](float) {}));
        animation->SetLoop(true);

        context.Tick(1.5f);
        EXPECT_EQ(context.GetAnimationCount(), 1u);
        EXPECT_TRUE(animation->GetTime() >= 0.0f && animation->GetTime() < 1.0f);
    }

} // namespace sky::ui
