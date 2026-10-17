//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>
#include <ui/widgets/Button.h>
#include <ui/widgets/Image.h>
#include <ui/widgets/Panel.h>

namespace sky::ui {

    namespace {

        UIStyle MakePanelStyle(uint32_t color)
        {
            UIStyle style;
            style.backgroundColor = color;
            return style;
        }

    } // namespace

    TEST(UIWidgetTest, PanelUsesThemeColor)
    {
        UITheme theme;
        theme.SetStyle("panel", MakePanelStyle(0xFF112233));

        UIPaintContext paint;
        paint.Begin({0.0f, 0.0f, 200.0f, 200.0f});
        paint.SetTheme(&theme);

        Panel panel;
        panel.AddStyleClass("panel");
        panel.SetBounds({0.0f, 0.0f, 100.0f, 100.0f});
        panel.Paint(paint);

        ASSERT_EQ(paint.GetDrawData().vertices.size(), 4u);
        EXPECT_EQ(paint.GetDrawData().vertices[0].color, 0xFF112233u);
    }

    TEST(UIWidgetTest, ThemeReplacementAffectsPanel)
    {
        UITheme theme;
        theme.SetStyle("panel", MakePanelStyle(0xFFFF0000));

        Panel panel;
        panel.AddStyleClass("panel");
        panel.SetBounds({0.0f, 0.0f, 10.0f, 10.0f});

        UIPaintContext first;
        first.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        first.SetTheme(&theme);
        panel.Paint(first);
        ASSERT_FALSE(first.GetDrawData().vertices.empty());
        EXPECT_EQ(first.GetDrawData().vertices[0].color, 0xFFFF0000u);

        theme.SetStyle("panel", MakePanelStyle(0xFF0000FF));

        UIPaintContext second;
        second.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        second.SetTheme(&theme);
        panel.Paint(second);
        ASSERT_FALSE(second.GetDrawData().vertices.empty());
        EXPECT_EQ(second.GetDrawData().vertices[0].color, 0xFF0000FFu);
    }

    TEST(UIWidgetTest, PanelClipsSubtreeViaContext)
    {
        UITheme theme;
        theme.SetStyle("panel", MakePanelStyle(0xFF00FF00));

        UIContext context;
        context.SetContentSize(200.0f, 200.0f);
        context.GetTheme().SetStyle("panel", MakePanelStyle(0xFF00FF00));

        auto panelHolder = std::make_unique<Panel>();
        auto *panel = panelHolder.get();
        context.AddChild(std::move(panelHolder));
        panel->AddStyleClass("panel");
        panel->SetBounds({0.0f, 0.0f, 100.0f, 100.0f});

        auto imageHolder = std::make_unique<Image>();
        auto *image = imageHolder.get();
        panel->AddChild(std::move(imageHolder));
        image->SetTexture(5);
        image->SetBounds({50.0f, 50.0f, 150.0f, 150.0f});

        UIPaintContext paint;
        context.Paint(paint);

        bool foundImage = false;
        for (const auto &cmd : paint.GetDrawData().commands) {
            if (cmd.textureId == 5) {
                foundImage = true;
                EXPECT_FLOAT_EQ(cmd.clip.right, 100.0f);
                EXPECT_FLOAT_EQ(cmd.clip.bottom, 100.0f);
            }
        }
        EXPECT_TRUE(foundImage);
    }

    TEST(UIWidgetTest, ImageBindsTextureHandle)
    {
        Image image;
        image.SetTexture(7);
        image.SetBounds({0.0f, 0.0f, 32.0f, 32.0f});

        UIPaintContext paint;
        paint.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        image.Paint(paint);

        ASSERT_EQ(paint.GetDrawData().commands.size(), 1u);
        EXPECT_EQ(paint.GetDrawData().commands[0].textureId, 7u);
    }

    TEST(UIWidgetTest, StylePerFieldCascade)
    {
        UITheme theme;
        UIStyle base;
        base.SetBackgroundColor(0xFFFF0000);
        theme.SetStyle("base", base);

        UIStyle overlay;
        overlay.SetBorderWidth(2.0f);
        theme.SetStyle("overlay", overlay);

        const UIStyle resolved = theme.Resolve({"base", "overlay"});
        EXPECT_EQ(resolved.backgroundColor, 0xFFFF0000u);
        EXPECT_FLOAT_EQ(resolved.borderWidth, 2.0f);
    }

    TEST(UIWidgetTest, NineSliceEmitsNineQuads)
    {
        Image image;
        image.SetTexture(3);
        image.SetBounds({0.0f, 0.0f, 30.0f, 30.0f});
        image.SetNineSlice(5.0f, 5.0f, 5.0f, 5.0f);

        UIPaintContext paint;
        paint.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        image.Paint(paint);

        EXPECT_EQ(paint.GetDrawData().vertices.size(), 36u);
        EXPECT_EQ(paint.GetDrawData().indices.size(), 54u);
    }

    TEST(UIWidgetTest, ImageWithoutTextureEmitsNothing)
    {
        Image image;
        image.SetBounds({0.0f, 0.0f, 32.0f, 32.0f});

        UIPaintContext paint;
        paint.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        image.Paint(paint);

        EXPECT_TRUE(paint.GetDrawData().commands.empty());
    }

    TEST(UIWidgetTest, ButtonClickInsideBounds)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto holder = std::make_unique<Button>();
        auto *button = holder.get();
        context.AddChild(std::move(holder));
        button->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});

        int clicks = 0;
        button->SetOnClick([&clicks]() { clicks++; });

        UIEventRouter router(context);

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        router.DispatchPointer(down);

        UIPointerEvent up;
        up.action = UIPointerAction::UP;
        up.x = 10.0f;
        up.y = 10.0f;
        router.DispatchPointer(up);

        EXPECT_EQ(clicks, 1);
    }

    TEST(UIWidgetTest, ButtonReleaseOutsideDoesNotClick)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto holder = std::make_unique<Button>();
        auto *button = holder.get();
        context.AddChild(std::move(holder));
        button->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});

        int clicks = 0;
        button->SetOnClick([&clicks]() { clicks++; });

        UIEventRouter router(context);

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        router.DispatchPointer(down);

        UIPointerEvent up;
        up.action = UIPointerAction::UP;
        up.x = 90.0f;
        up.y = 90.0f;
        router.DispatchPointer(up);

        EXPECT_EQ(clicks, 0);
    }

    TEST(UIWidgetTest, DisabledButtonIgnoresInput)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto holder = std::make_unique<Button>();
        auto *button = holder.get();
        context.AddChild(std::move(holder));
        button->SetBounds({0.0f, 0.0f, 50.0f, 50.0f});
        button->SetEnabled(false);

        int clicks = 0;
        button->SetOnClick([&clicks]() { clicks++; });

        UIEventRouter router(context);

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        EXPECT_EQ(router.DispatchPointer(down), UIEventResult::UNHANDLED);

        UIPointerEvent up;
        up.action = UIPointerAction::UP;
        up.x = 10.0f;
        up.y = 10.0f;
        router.DispatchPointer(up);

        EXPECT_EQ(clicks, 0);
        EXPECT_EQ(button->GetState(), Button::State::Disabled);
    }

    TEST(UIWidgetTest, PressedButtonUsesPressedColor)
    {
        UITheme theme;
        UIStyle style;
        style.buttonNormal  = 0xFF111111;
        style.buttonPressed = 0xFF222222;
        theme.SetStyle("btn", style);

        Button button;
        button.AddStyleClass("btn");
        button.SetBounds({0.0f, 0.0f, 40.0f, 40.0f});

        UIPointerEvent down;
        down.action = UIPointerAction::DOWN;
        down.x = 10.0f;
        down.y = 10.0f;
        button.OnPointerEvent(down);
        ASSERT_EQ(button.GetState(), Button::State::Pressed);

        UIPaintContext paint;
        paint.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        paint.SetTheme(&theme);
        button.Paint(paint);

        ASSERT_FALSE(paint.GetDrawData().vertices.empty());
        EXPECT_EQ(paint.GetDrawData().vertices[0].color, 0xFF222222u);
    }

} // namespace sky::ui
