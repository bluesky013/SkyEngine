//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/freetype/FreeTypeUIFontProvider.h>

namespace sky::ui {

    TEST(UITextFreeTypeTest, LoadsRealFont)
    {
        FreeTypeUIFontProvider provider;
        ASSERT_TRUE(provider.LoadFont("assets/fonts/OpenSans-Regular.ttf"));
        EXPECT_TRUE(provider.IsReady());

        UIGlyphBitmap glyph;
        ASSERT_TRUE(provider.GetGlyph('A', 24, glyph));
        EXPECT_GT(glyph.width, 0u);
        EXPECT_GT(glyph.height, 0u);
        EXPECT_GT(glyph.advance, 0.0f);
        EXPECT_FALSE(glyph.pixels.empty());

        const UIFontMetrics metrics = provider.GetFontMetrics(24);
        EXPECT_GT(metrics.lineHeight, 0.0f);
        EXPECT_GT(metrics.ascender, 0.0f);
    }

    TEST(UITextFreeTypeTest, MissingFileFails)
    {
        FreeTypeUIFontProvider provider;
        EXPECT_FALSE(provider.LoadFont("assets/fonts/does-not-exist.ttf"));
        EXPECT_FALSE(provider.IsReady());
    }

} // namespace sky::ui
