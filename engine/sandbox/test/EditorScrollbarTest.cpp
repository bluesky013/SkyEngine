#include <editor/ui/UiTheme.h>
#include <editor/widgets/UiSkin.h>

#include <ui/UIPaintContext.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::editor {
    namespace {
        ui::UIDrawData PaintScrollbar(const UiTheme &theme, const ui::UIRect &track, float contentHeight, float scrollTop)
        {
            ui::UIPaintContext paint;
            // Leave room outside the track so a misplaced thumb cannot pass by clipping.
            paint.Begin({track.left - 100.0f, track.top - 100.0f, track.right + 100.0f, track.bottom + 100.0f});
            UiSkin(theme, nullptr).DrawScrollbar(paint, track, contentHeight, scrollTop);
            return paint.GetDrawData();
        }

        ui::UIRect DrawBounds(const ui::UIDrawData &draw)
        {
            const float infinity = std::numeric_limits<float>::infinity();
            ui::UIRect bounds{infinity, infinity, -infinity, -infinity};
            for (const auto &vertex : draw.vertices) {
                bounds.left   = std::min(bounds.left, vertex.x);
                bounds.top    = std::min(bounds.top, vertex.y);
                bounds.right  = std::max(bounds.right, vertex.x);
                bounds.bottom = std::max(bounds.bottom, vertex.y);
            }
            return bounds;
        }

        void ExpectThumbInside(const ui::UIDrawData &draw, const ui::UIRect &track, uint32_t color)
        {
            ASSERT_FALSE(draw.vertices.empty());
            ASSERT_FALSE(draw.indices.empty());
            for (const auto &vertex : draw.vertices) {
                EXPECT_TRUE(std::isfinite(vertex.x));
                EXPECT_TRUE(std::isfinite(vertex.y));
                EXPECT_GE(vertex.x, track.left);
                EXPECT_LE(vertex.x, track.right);
                EXPECT_GE(vertex.y, track.top);
                EXPECT_LE(vertex.y, track.bottom);
                EXPECT_EQ(vertex.color, color);
            }
            const auto bounds = DrawBounds(draw);
            EXPECT_GT(bounds.Width(), 0.0f);
            EXPECT_GT(bounds.Height(), 0.0f);
        }
    } // namespace

    TEST(EditorScrollbarTest, ThumbStaysInsideReservedTrackAcrossScales)
    {
        for (float scale : {0.5f, 1.0f, 2.0f}) {
            SCOPED_TRACE(scale);
            const auto theme = MakeDarkTheme(scale);
            const ui::UIRect track{120.0f, 30.0f, 120.0f + theme.metrics.scrollBarWidth, 30.0f + 180.0f * scale};
            const float contentHeight = track.Height() * 4.0f;
            const float maxScroll = contentHeight - track.Height();
            for (float scroll : {0.0f, maxScroll * 0.5f, maxScroll}) {
                SCOPED_TRACE(scroll);
                ExpectThumbInside(PaintScrollbar(theme, track, contentHeight, scroll), track, theme.colors.scrollBar);
            }
        }
    }

    TEST(EditorScrollbarTest, ThumbMovesDownAsContentScrolls)
    {
        for (float scale : {0.5f, 1.0f, 2.0f}) {
            SCOPED_TRACE(scale);
            const auto theme = MakeDarkTheme(scale);
            const ui::UIRect track{40.0f, 20.0f, 40.0f + theme.metrics.scrollBarWidth, 20.0f + 180.0f * scale};
            const float contentHeight = track.Height() * 4.0f;
            const float maxScroll = contentHeight - track.Height();
            const auto first = PaintScrollbar(theme, track, contentHeight, 0.0f);
            const auto middle = PaintScrollbar(theme, track, contentHeight, maxScroll * 0.5f);
            const auto last = PaintScrollbar(theme, track, contentHeight, maxScroll);
            ASSERT_FALSE(first.vertices.empty());
            ASSERT_FALSE(middle.vertices.empty());
            ASSERT_FALSE(last.vertices.empty());
            EXPECT_LT(DrawBounds(first).top, DrawBounds(middle).top);
            EXPECT_LT(DrawBounds(middle).top, DrawBounds(last).top);
        }
    }

    TEST(EditorScrollbarTest, NarrowTrackStillContainsTheThumb)
    {
        const auto theme = MakeDarkTheme(2.0f);
        const ui::UIRect track{80.0f, 25.0f, 81.0f, 175.0f};
        ExpectThumbInside(PaintScrollbar(theme, track, track.Height() * 3.0f, track.Height()), track, theme.colors.scrollBar);
    }

    TEST(EditorScrollbarTest, ContentThatFitsEmitsNoThumb)
    {
        for (float scale : {0.5f, 1.0f, 2.0f}) {
            SCOPED_TRACE(scale);
            const auto theme = MakeDarkTheme(scale);
            const ui::UIRect track{40.0f, 20.0f, 40.0f + theme.metrics.scrollBarWidth, 20.0f + 180.0f * scale};
            for (float contentHeight : {0.0f, track.Height() * 0.75f, track.Height()}) {
                const auto draw = PaintScrollbar(theme, track, contentHeight, 0.0f);
                EXPECT_TRUE(draw.vertices.empty());
                EXPECT_TRUE(draw.indices.empty());
                EXPECT_TRUE(draw.commands.empty());
            }
        }
    }
} // namespace sky::editor
