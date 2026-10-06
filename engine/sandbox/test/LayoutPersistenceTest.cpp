//
// Created on 2026/09/21.
//

#include <editor/core/layout/LayoutPersistence.h>
#include <editor/core/layout/PanelRegistry.h>
#include <filesystem>
#include <fstream>
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

TEST(LayoutPersistenceTest, FloatingRoundTrip)
{
    LayoutModel model;
    model.SetDefault({"outliner"});
    ASSERT_TRUE(model.SplitPanel("outliner", SplitOrientation::HORIZONTAL, "inspector"));

    FloatingPanel geometry;
    geometry.x = 12.0f;
    geometry.y = 34.0f;
    geometry.width = 400.0f;
    geometry.height = 300.0f;
    geometry.active = true;
    ASSERT_TRUE(model.FloatPanel("inspector", geometry));

    const std::string path = "test_editor_layout_floating.json";
    ASSERT_TRUE(LayoutPersistence::Save(model, path));

    LayoutModel restored;
    ASSERT_TRUE(LayoutPersistence::Load(path, restored));
    EXPECT_EQ(restored.GetVersion(), 2);
    const FloatingPanel *fp = restored.FindFloating("inspector");
    ASSERT_NE(fp, nullptr);
    EXPECT_FLOAT_EQ(fp->x, 12.0f);
    EXPECT_FLOAT_EQ(fp->y, 34.0f);
    EXPECT_FLOAT_EQ(fp->width, 400.0f);
    EXPECT_FLOAT_EQ(fp->height, 300.0f);
    EXPECT_TRUE(fp->active);

    std::filesystem::remove(path);
}

TEST(LayoutPersistenceTest, LoadVersion1WithoutFloating)
{
    const std::string path = "test_editor_layout_v1.json";
    {
        std::ofstream file(path, std::ios::binary);
        ASSERT_TRUE(file.is_open());
        file << R"({"version":1,"root":{"type":"tab","panels":["a","b"],"active":0}})";
    }

    LayoutModel restored;
    ASSERT_TRUE(LayoutPersistence::Load(path, restored));
    EXPECT_EQ(restored.GetVersion(), 1);
    EXPECT_TRUE(restored.GetFloatingPanels().empty());
    EXPECT_EQ(Panels(restored).size(), 2u);

    std::filesystem::remove(path);
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
