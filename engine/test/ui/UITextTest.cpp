//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UIFontAtlas.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/Text.h>

namespace sky::ui {

    namespace {

        class FakeTextureRegistry : public IUITextureRegistry {
        public:
            UITextureId RegisterTexture(const UIImageData &image) override
            {
                (void)image;
                registrations++;
                return next++;
            }

            void UpdateTexture(UITextureId id, const UIImageData &image) override
            {
                (void)id;
                (void)image;
                updates++;
            }

            void ReleaseTexture(UITextureId id) override { (void)id; }

            UITextureId next = 1;
            int registrations = 0;
            int updates = 0;
        };

    } // namespace

    TEST(UITextTest, MeasureSingleLine)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas atlas(&registry);
        atlas.SetProvider(&provider);

        const UITextExtent extent = UITextLayout::Measure("AB", 10, atlas);
        // advance = floor(0.6 * 10) = 6 per glyph.
        EXPECT_FLOAT_EQ(extent.width, 12.0f);
        EXPECT_FLOAT_EQ(extent.height, 10.0f);
    }

    TEST(UITextTest, MeasureMultiLine)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas atlas(&registry);
        atlas.SetProvider(&provider);

        const UITextExtent extent = UITextLayout::Measure("A\nB", 10, atlas);
        EXPECT_FLOAT_EQ(extent.width, 6.0f);
        EXPECT_FLOAT_EQ(extent.height, 20.0f);
    }

    TEST(UITextTest, AtlasRegistersPageAndReusesGlyph)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas atlas(&registry);
        atlas.SetProvider(&provider);

        const UIGlyphEntry &first = atlas.GetGlyph('A', 10);
        EXPECT_NE(first.textureId, UI_INVALID_TEXTURE);
        EXPECT_EQ(registry.registrations, 1);

        const UIGlyphEntry &again = atlas.GetGlyph('A', 10);
        EXPECT_EQ(again.textureId, first.textureId);
        EXPECT_EQ(registry.registrations, 1);
    }

    TEST(UITextTest, AtlasGrowthKeepsHandles)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas atlas(&registry, 16);
        atlas.SetProvider(&provider);

        // size 10 -> glyph 5x10; a 16x16 page holds three, the fourth needs a new page.
        const UIGlyphEntry &first = atlas.GetGlyph('A', 10);
        atlas.GetGlyph('B', 10);
        atlas.GetGlyph('C', 10);
        atlas.GetGlyph('D', 10);

        EXPECT_GE(atlas.GetPageCount(), 2u);
        EXPECT_GE(registry.registrations, 2);
        // Previously emitted handles stay valid.
        EXPECT_NE(first.textureId, UI_INVALID_TEXTURE);
        EXPECT_EQ(atlas.GetGlyph('A', 10).textureId, first.textureId);
    }

    TEST(UITextTest, EmitReferencesAtlasHandle)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas atlas(&registry);
        atlas.SetProvider(&provider);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        UITextLayout::Emit(context, "A", 10, 0.0f, 0.0f, 0xFFFFFFFF, atlas);

        ASSERT_FALSE(context.GetDrawData().commands.empty());
        EXPECT_NE(context.GetDrawData().commands[0].textureId, UI_INVALID_TEXTURE);
    }

    TEST(UITextTest, EmitRespectsClip)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UIFontAtlas atlas(&registry);
        atlas.SetProvider(&provider);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        context.PushClip({0.0f, 0.0f, 3.0f, 3.0f});
        UITextLayout::Emit(context, "A", 10, 0.0f, 0.0f, 0xFFFFFFFF, atlas);

        ASSERT_FALSE(context.GetDrawData().commands.empty());
        const UIRect &clip = context.GetDrawData().commands[0].clip;
        EXPECT_FLOAT_EQ(clip.right, 3.0f);
        EXPECT_FLOAT_EQ(clip.bottom, 3.0f);
    }

    TEST(UITextTest, WidgetMeasureAndPaint)
    {
        FakeTextureRegistry registry;
        UIBuiltinFontProvider provider;
        UITextSystem textSystem(&provider, &registry);

        Text text;
        text.SetTextSystem(&textSystem);
        text.SetContent("AB");
        text.SetFontSize(10);

        float width = 0.0f;
        float height = 0.0f;
        text.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 12.0f);
        EXPECT_FLOAT_EQ(height, 10.0f);

        text.SetBounds({0.0f, 0.0f, 100.0f, 100.0f});

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        EXPECT_FALSE(context.GetDrawData().commands.empty());
    }

    TEST(UITextTest, WidgetWithoutSystemIsNoOp)
    {
        Text text;
        text.SetContent("AB");

        float width = 7.0f;
        float height = 7.0f;
        text.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 0.0f);
        EXPECT_FLOAT_EQ(height, 0.0f);

        UIPaintContext context;
        context.Begin({0.0f, 0.0f, 100.0f, 100.0f});
        text.Paint(context);
        EXPECT_TRUE(context.GetDrawData().commands.empty());
    }

} // namespace sky::ui
