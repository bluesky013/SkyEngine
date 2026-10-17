//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>
#include <ui/UITransform.h>
#include <ui/widgets/Image.h>

namespace sky::ui {

    TEST(UITransformTest, ContextTransformTranslatesVertices)
    {
        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});

        UI2DTransform translate;
        translate.m02 = 10.0f;
        context.PushTransform(translate);
        context.AddRect({0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFF);
        context.PopTransform();

        ASSERT_EQ(context.GetDrawData().vertices.size(), 4u);
        EXPECT_FLOAT_EQ(context.GetDrawData().vertices[0].x, 10.0f);
    }

    TEST(UITransformTest, ContextOpacityScalesAlpha)
    {
        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});

        context.PushOpacity(0.5f);
        context.AddRect({0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFF);
        context.PopOpacity();

        ASSERT_EQ(context.GetDrawData().vertices.size(), 4u);
        EXPECT_EQ(context.GetDrawData().vertices[0].color, 0x80FFFFFFu);
    }

    TEST(UITransformTest, ParentTransformAffectsChild)
    {
        UIElement parent;
        UITransform transform;
        transform.translationX = 5.0f;
        parent.SetTransform(transform);

        auto image = std::make_unique<Image>();
        image->SetTexture(1);
        image->SetBounds({0.0f, 0.0f, 10.0f, 10.0f});
        parent.AddChild(std::move(image));

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        parent.Paint(context);

        ASSERT_FALSE(context.GetDrawData().vertices.empty());
        EXPECT_FLOAT_EQ(context.GetDrawData().vertices[0].x, 5.0f);
    }

    TEST(UITransformTest, ParentOpacityCascades)
    {
        UIElement parent;
        parent.SetOpacity(0.5f);

        auto image = std::make_unique<Image>();
        image->SetTexture(1);
        image->SetTint(0xFFFFFFFF);
        image->SetBounds({0.0f, 0.0f, 10.0f, 10.0f});
        parent.AddChild(std::move(image));

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        parent.Paint(context);

        ASSERT_FALSE(context.GetDrawData().vertices.empty());
        EXPECT_EQ(context.GetDrawData().vertices[0].color, 0x80FFFFFFu);
    }

} // namespace sky::ui
