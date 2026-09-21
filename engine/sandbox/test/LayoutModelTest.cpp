//
// Created on 2026/09/21.
//

#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace sky::editor;

namespace {

    std::vector<std::string> Panels(const LayoutModel &model)
    {
        std::vector<std::string> out;
        model.CollectPanels(out);
        return out;
    }

    bool Contains(const std::vector<std::string> &values, const std::string &value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

} // namespace

TEST(LayoutModelTest, SingleTabAndCollect)
{
    LayoutModel model;
    model.SetDefault({"outliner", "inspector"});

    ASSERT_FALSE(model.IsEmpty());
    const auto panels = Panels(model);
    ASSERT_EQ(panels.size(), 2u);
    EXPECT_EQ(panels[0], "outliner");
    EXPECT_EQ(panels[1], "inspector");

    EXPECT_NE(model.FindTab("inspector"), nullptr);
    EXPECT_EQ(model.FindTab("missing"), nullptr);
}

TEST(LayoutModelTest, SplitPanel)
{
    LayoutModel model;
    model.SetDefault({"viewport"});
    ASSERT_TRUE(model.SplitPanel("viewport", SplitOrientation::VERTICAL, "console"));

    const auto panels = Panels(model);
    ASSERT_EQ(panels.size(), 2u);
    EXPECT_TRUE(Contains(panels, "viewport"));
    EXPECT_TRUE(Contains(panels, "console"));

    auto *root = model.GetRoot();
    ASSERT_TRUE(IsSplit(root));
    auto *split = static_cast<SplitNode *>(root);
    EXPECT_EQ(split->orientation, SplitOrientation::VERTICAL);
    ASSERT_EQ(split->children.size(), 2u);
    ASSERT_EQ(split->ratios.size(), 2u);
    EXPECT_FLOAT_EQ(split->ratios[0], 0.5f);
}

TEST(LayoutModelTest, SetRatioNormalizes)
{
    LayoutModel model;
    model.SetDefault({"a"});
    ASSERT_TRUE(model.SplitPanel("a", SplitOrientation::HORIZONTAL, "b"));

    auto *root = model.GetRoot();
    ASSERT_TRUE(IsSplit(root));
    ASSERT_TRUE(model.SetRatio(root, 0, 0.75f));

    auto *split = static_cast<SplitNode *>(root);
    ASSERT_EQ(split->ratios.size(), 2u);
    EXPECT_NEAR(split->ratios[0], 0.75f, 1e-4f);
    EXPECT_NEAR(split->ratios[1], 0.25f, 1e-4f);
}

TEST(LayoutModelTest, TabifyAndMove)
{
    LayoutModel model;
    model.SetDefault({"a"});
    ASSERT_TRUE(model.SplitPanel("a", SplitOrientation::HORIZONTAL, "b"));

    ASSERT_TRUE(model.MoveToArea("b", "a"));
    TabNode *tab = model.FindTab("a");
    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(tab, model.FindTab("b"));
    EXPECT_EQ(tab->panels.size(), 2u);
}

TEST(LayoutModelTest, CloseCollapses)
{
    LayoutModel model;
    model.SetDefault({"a"});
    ASSERT_TRUE(model.SplitPanel("a", SplitOrientation::HORIZONTAL, "b"));
    ASSERT_TRUE(model.ClosePanel("b"));

    // The split collapses back to a single tab.
    EXPECT_TRUE(IsTab(model.GetRoot()));
    const auto panels = Panels(model);
    ASSERT_EQ(panels.size(), 1u);
    EXPECT_EQ(panels[0], "a");

    ASSERT_TRUE(model.ClosePanel("a"));
    EXPECT_TRUE(model.IsEmpty());
}

TEST(LayoutModelTest, ResetToDefault)
{
    LayoutModel model;
    model.SetDefault({"a", "b"});
    ASSERT_TRUE(model.SplitPanel("a", SplitOrientation::VERTICAL, "c"));
    ASSERT_TRUE(model.ClosePanel("b"));

    model.ResetToDefault();
    const auto panels = Panels(model);
    ASSERT_EQ(panels.size(), 2u);
    EXPECT_TRUE(Contains(panels, "a"));
    EXPECT_TRUE(Contains(panels, "b"));
}

TEST(LayoutModelTest, JsonRoundTrip)
{
    LayoutModel model;
    model.SetDefault({"a"});
    ASSERT_TRUE(model.SplitPanel("a", SplitOrientation::VERTICAL, "b"));
    ASSERT_TRUE(model.SplitPanel("b", SplitOrientation::HORIZONTAL, "c"));

    const std::string json = model.ToJson(2);

    LayoutModel restored;
    std::vector<std::string> warnings;
    ASSERT_TRUE(LayoutModel::FromJson(json, restored, nullptr, &warnings));
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(Panels(restored), Panels(model));
}

TEST(LayoutModelTest, JsonSkipsUnknownPanels)
{
    LayoutModel model;
    model.SetDefault({"a", "b", "c"});
    const std::string json = model.ToJson();

    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.f, 0.f, nullptr});
    registry.Register(PanelInfo{"c", "C", 0.f, 0.f, nullptr});

    LayoutModel restored;
    std::vector<std::string> warnings;
    ASSERT_TRUE(LayoutModel::FromJson(json, restored, &registry, &warnings));

    const auto panels = Panels(restored);
    ASSERT_EQ(panels.size(), 2u);
    EXPECT_TRUE(Contains(panels, "a"));
    EXPECT_TRUE(Contains(panels, "c"));
    EXPECT_EQ(warnings.size(), 1u);
}

TEST(LayoutModelTest, FromJsonRejectsGarbage)
{
    LayoutModel model;
    EXPECT_FALSE(LayoutModel::FromJson("not json", model));
}

TEST(PanelRegistryTest, RegisterFindUnregister)
{
    PanelRegistry registry;
    registry.Register(PanelInfo{"outliner", "Outliner", 100.f, 80.f, nullptr});

    ASSERT_TRUE(registry.Contains("outliner"));
    const PanelInfo *info = registry.Find("outliner");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->title, "Outliner");
    EXPECT_FLOAT_EQ(info->minWidth, 100.f);
    EXPECT_EQ(registry.Find("missing"), nullptr);

    EXPECT_TRUE(registry.Unregister("outliner"));
    EXPECT_FALSE(registry.Contains("outliner"));
}
