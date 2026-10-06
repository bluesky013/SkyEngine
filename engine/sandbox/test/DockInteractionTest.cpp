//
// Created on 2026/10/06.
//

#include <editor/core/layout/DockInteraction.h>
#include <gtest/gtest.h>
#include <memory>

using namespace sky::editor;

namespace {

    std::unique_ptr<SplitNode> MakeSplit(SplitOrientation orientation, uint32_t count)
    {
        auto split = std::make_unique<SplitNode>();
        split->orientation = orientation;
        split->ratios.assign(count, 1.0f / static_cast<float>(count));
        for (uint32_t i = 0; i < count; ++i) {
            split->children.push_back(std::make_unique<TabNode>());
        }
        return split;
    }

} // namespace

TEST(DockInteractionTest, ComputeChildRectsEvenly)
{
    auto split = MakeSplit(SplitOrientation::HORIZONTAL, 2);
    std::vector<LayoutRect> out;
    ComputeChildRects(*split, LayoutRect{0.0f, 0.0f, 100.0f, 50.0f}, out);

    ASSERT_EQ(out.size(), 2u);
    EXPECT_FLOAT_EQ(out[0].left, 0.0f);
    EXPECT_FLOAT_EQ(out[0].right, 50.0f);
    EXPECT_FLOAT_EQ(out[1].left, 50.0f);
    EXPECT_FLOAT_EQ(out[1].right, 100.0f);
}

TEST(DockInteractionTest, CollectSplitterBandsVertical)
{
    auto split = MakeSplit(SplitOrientation::VERTICAL, 2);
    std::vector<SplitterBand> bands;
    CollectSplitterBands(split.get(), LayoutRect{0.0f, 0.0f, 100.0f, 50.0f}, 4.0f, bands);

    ASSERT_EQ(bands.size(), 1u);
    EXPECT_EQ(bands[0].orientation, SplitOrientation::VERTICAL);
    EXPECT_EQ(bands[0].index, 0u);
    EXPECT_NEAR(bands[0].rect.top, 23.0f, 0.01f);
    EXPECT_NEAR(bands[0].rect.bottom, 27.0f, 0.01f);
    EXPECT_FLOAT_EQ(bands[0].rect.left, 0.0f);
    EXPECT_FLOAT_EQ(bands[0].rect.right, 100.0f);
}

TEST(DockInteractionTest, CollectNestedSplitterBands)
{
    auto root = MakeSplit(SplitOrientation::HORIZONTAL, 2);
    auto nested = MakeSplit(SplitOrientation::VERTICAL, 2);
    root->children[1] = std::move(nested);

    std::vector<SplitterBand> bands;
    CollectSplitterBands(root.get(), LayoutRect{0.0f, 0.0f, 100.0f, 50.0f}, 4.0f, bands);

    // one band at the top-level horizontal seam, one in the nested vertical split.
    ASSERT_EQ(bands.size(), 2u);
    EXPECT_EQ(bands[0].orientation, SplitOrientation::HORIZONTAL);
    EXPECT_EQ(bands[1].orientation, SplitOrientation::VERTICAL);
}

TEST(DockInteractionTest, RatioFromDrag)
{
    auto split = MakeSplit(SplitOrientation::HORIZONTAL, 2);
    SplitterBand band;
    band.split = split.get();
    band.index = 0;
    band.orientation = SplitOrientation::HORIZONTAL;
    band.parentRect = LayoutRect{0.0f, 0.0f, 100.0f, 50.0f};

    EXPECT_NEAR(RatioFromDrag(band, 70.0f, 10.0f), 0.7f, 1e-4f);
    EXPECT_NEAR(RatioFromDrag(band, 30.0f, 10.0f), 0.3f, 1e-4f);
}

TEST(DockInteractionTest, ResolveDockPosition)
{
    const LayoutRect rect{0.0f, 0.0f, 100.0f, 100.0f};
    EXPECT_EQ(ResolveDockPosition(rect, 5.0f, 50.0f, 0.25f), DockPosition::LEFT);
    EXPECT_EQ(ResolveDockPosition(rect, 95.0f, 50.0f, 0.25f), DockPosition::RIGHT);
    EXPECT_EQ(ResolveDockPosition(rect, 50.0f, 5.0f, 0.25f), DockPosition::TOP);
    EXPECT_EQ(ResolveDockPosition(rect, 50.0f, 95.0f, 0.25f), DockPosition::BOTTOM);
    EXPECT_EQ(ResolveDockPosition(rect, 50.0f, 50.0f, 0.25f), DockPosition::CENTER);
}
