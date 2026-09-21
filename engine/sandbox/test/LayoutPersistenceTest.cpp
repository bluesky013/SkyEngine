//
// Created on 2026/09/21.
//

#include <editor/core/layout/LayoutPersistence.h>
#include <editor/core/layout/PanelRegistry.h>
#include <filesystem>
#include <gtest/gtest.h>
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

} // namespace

TEST(LayoutPersistenceTest, SaveLoadRoundTrip)
{
    LayoutModel model;
    model.SetDefault({"outliner"});
    ASSERT_TRUE(model.SplitPanel("outliner", SplitOrientation::VERTICAL, "inspector"));
    ASSERT_TRUE(model.SplitPanel("inspector", SplitOrientation::HORIZONTAL, "console"));

    const std::string path = "test_editor_layout.json";
    ASSERT_TRUE(LayoutPersistence::Save(model, path));

    LayoutModel restored;
    ASSERT_TRUE(LayoutPersistence::Load(path, restored));
    EXPECT_EQ(Panels(restored), Panels(model));

    std::filesystem::remove(path);
}

TEST(LayoutPersistenceTest, LoadMissingFileFails)
{
    LayoutModel model;
    EXPECT_FALSE(LayoutPersistence::Load("does_not_exist_layout.json", model));
}

TEST(LayoutPersistenceTest, SaveEmptyPathFails)
{
    LayoutModel model;
    model.SetDefault({"a"});
    EXPECT_FALSE(LayoutPersistence::Save(model, ""));
    EXPECT_FALSE(LayoutPersistence::Load("", model));
}

TEST(LayoutPersistenceTest, LoadWithRegistrySkipsUnknown)
{
    LayoutModel model;
    model.SetDefault({"a", "b"});
    const std::string path = "test_editor_layout_registry.json";
    ASSERT_TRUE(LayoutPersistence::Save(model, path));

    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.f, 0.f, nullptr});

    LayoutModel restored;
    std::vector<std::string> warnings;
    ASSERT_TRUE(LayoutPersistence::Load(path, restored, &registry, &warnings));
    const auto panels = Panels(restored);
    ASSERT_EQ(panels.size(), 1u);
    EXPECT_EQ(panels[0], "a");
    EXPECT_EQ(warnings.size(), 1u);

    std::filesystem::remove(path);
}
