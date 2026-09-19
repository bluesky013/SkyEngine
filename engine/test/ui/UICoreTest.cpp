//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UILayout.h>
#include <ui/UIPaintContext.h>
#include <ui/UIStyle.h>

namespace sky::ui {

    TEST(UICoreTest, AddAndRemoveChild)
    {
        UIElement parent;
        UIElement *child = parent.AddChild(std::make_unique<UIElement>());

        ASSERT_EQ(parent.GetChildren().size(), 1u);
        EXPECT_EQ(child->GetParent(), &parent);

        UIElementPtr removed = parent.RemoveChild(child);
        EXPECT_TRUE(parent.GetChildren().empty());
        EXPECT_EQ(removed->GetParent(), nullptr);
    }

    TEST(UICoreTest, SingleRoot)
    {
        UIContext context;
        ASSERT_NE(context.GetRoot(), nullptr);

        UIElement *a = context.AddChild(std::make_unique<UIElement>());
        UIElement *b = context.AddChild(std::make_unique<UIElement>());

        EXPECT_EQ(a->GetParent(), context.GetRoot());
        EXPECT_EQ(b->GetParent(), context.GetRoot());
        EXPECT_EQ(context.GetRoot()->GetChildren().size(), 2u);
    }

    TEST(UICoreTest, FindByName)
    {
        UIContext context;
        UIElement *a = context.AddChild(std::make_unique<UIElement>());
        a->SetName("Panel");

        UIElement *found = context.FindByName("Panel");
        EXPECT_EQ(found, a);
        EXPECT_EQ(context.FindByName("Missing"), nullptr);
    }

    TEST(UICoreTest, VisibilityHierarchy)
    {
        UIContext context;
        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        UIElement *child = parent->AddChild(std::make_unique<UIElement>());

        EXPECT_TRUE(child->IsEffectivelyVisible());

        parent->SetVisible(false);
        EXPECT_FALSE(parent->IsEffectivelyVisible());
        EXPECT_FALSE(child->IsEffectivelyVisible());
    }

    TEST(UICoreTest, DirtyPropagation)
    {
        UIContext context;
        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        UIElement *child = parent->AddChild(std::make_unique<UIElement>());
        UIElement *sibling = context.AddChild(std::make_unique<UIElement>());

        context.Traverse([](UIElement *element) {
            element->ClearLayoutDirty();
            element->ClearPaintDirty();
        });

        UIRect bounds;
        bounds.right  = 100.0f;
        bounds.bottom = 50.0f;
        parent->SetBounds(bounds);

        EXPECT_TRUE(parent->IsLayoutDirty());
        EXPECT_TRUE(child->IsLayoutDirty());
        EXPECT_FALSE(sibling->IsLayoutDirty());
    }

    TEST(UICoreTest, CleanSubtreeStaysClean)
    {
        UIContext context;
        UIElement *a = context.AddChild(std::make_unique<UIElement>());
        UIElement *b = context.AddChild(std::make_unique<UIElement>());

        context.Traverse([](UIElement *element) {
            element->ClearLayoutDirty();
            element->ClearPaintDirty();
        });

        a->MarkLayoutDirty();
        EXPECT_TRUE(a->IsLayoutDirty());
        EXPECT_FALSE(b->IsLayoutDirty());
    }

    TEST(UICoreTest, FixedSizeArrange)
    {
        UIContext context;
        context.SetContentSize(800.0f, 600.0f);

        UIElement *element = context.AddChild(std::make_unique<UIElement>());
        UILayoutParams layout;
        layout.width.mode  = UISizeMode::FIXED;
        layout.width.value = 120.0f;
        layout.height.mode  = UISizeMode::FIXED;
        layout.height.value = 40.0f;
        element->SetLayout(layout);

        context.Layout();

        EXPECT_FLOAT_EQ(element->GetBounds().Width(), 120.0f);
        EXPECT_FLOAT_EQ(element->GetBounds().Height(), 40.0f);
    }

    TEST(UICoreTest, ParentFillShrinksToContent)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);

        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        UILayoutParams parentLayout;
        parentLayout.width.mode  = UISizeMode::FIXED;
        parentLayout.width.value = 200.0f;
        parentLayout.height.mode  = UISizeMode::FIXED;
        parentLayout.height.value = 100.0f;
        parentLayout.paddingLeft   = 10.0f;
        parentLayout.paddingRight  = 10.0f;
        parent->SetLayout(parentLayout);

        UIElement *child = parent->AddChild(std::make_unique<UIElement>());
        UILayoutParams childLayout;
        childLayout.width.mode  = UISizeMode::FILL;
        childLayout.height.mode = UISizeMode::FILL;
        child->SetLayout(childLayout);

        context.Layout();

        EXPECT_FLOAT_EQ(child->GetBounds().left, 10.0f);
        EXPECT_FLOAT_EQ(child->GetBounds().Width(), 180.0f);
        EXPECT_FLOAT_EQ(child->GetBounds().Height(), 100.0f);
    }

    TEST(UICoreTest, ThemeChangeAffectsResolvedStyle)
    {
        UITheme theme;
        UIStyle red;
        red.backgroundColor = 0xFFFF0000;
        theme.SetStyle("panel", red);

        std::vector<std::string> classes = {"panel"};
        EXPECT_EQ(theme.Resolve(classes).backgroundColor, 0xFFFF0000u);

        UIStyle blue;
        blue.backgroundColor = 0xFF0000FF;
        theme.SetStyle("panel", blue);

        EXPECT_EQ(theme.Resolve(classes).backgroundColor, 0xFF0000FFu);
    }

    TEST(UICoreTest, ClipIntersectionInDrawCommand)
    {
        UIPaintContext context;
        UIRect surface{0.0f, 0.0f, 100.0f, 100.0f};
        context.Begin(surface);

        context.PushClip({0.0f, 0.0f, 60.0f, 60.0f});
        context.PushClip({50.0f, 50.0f, 200.0f, 200.0f});
        context.AddRect({0.0f, 0.0f, 100.0f, 100.0f}, 0xFFFFFFFF);

        ASSERT_EQ(context.GetDrawData().commands.size(), 1u);
        const UIRect &clip = context.GetDrawData().commands[0].clip;
        EXPECT_FLOAT_EQ(clip.left, 50.0f);
        EXPECT_FLOAT_EQ(clip.top, 50.0f);
        EXPECT_FLOAT_EQ(clip.right, 60.0f);
        EXPECT_FLOAT_EQ(clip.bottom, 60.0f);
    }

    TEST(UICoreTest, TextureSwitchSplitsCommands)
    {
        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});

        context.AddTexturedQuad({0.0f, 0.0f, 10.0f, 10.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, 1, 0xFFFFFFFF);
        context.AddTexturedQuad({10.0f, 0.0f, 20.0f, 10.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, 2, 0xFFFFFFFF);

        EXPECT_EQ(context.GetDrawData().commands.size(), 2u);
    }

    TEST(UICoreTest, HiddenElementSkipsPaint)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        UILayoutParams layout;
        layout.width.mode  = UISizeMode::FIXED;
        layout.width.value = 100.0f;
        layout.height.mode  = UISizeMode::FIXED;
        layout.height.value = 100.0f;
        parent->SetLayout(layout);

        UIElement *child = parent->AddChild(std::make_unique<UIElement>());
        UILayoutParams childLayout;
        childLayout.width.mode  = UISizeMode::FIXED;
        childLayout.width.value = 50.0f;
        childLayout.height.mode  = UISizeMode::FIXED;
        childLayout.height.value = 50.0f;
        child->SetLayout(childLayout);

        context.Layout();
        parent->SetVisible(false);

        UIPaintContext paint;
        context.Paint(paint);

        EXPECT_TRUE(paint.GetDrawData().commands.empty());
    }

} // namespace sky::ui
