//
// Created on 2026/10/06.
//

#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/shell/ShellModels.h>
#include <gtest/gtest.h>

using namespace sky::editor;

TEST(ShellModelsTest, ViewMenuItemsReflectLayout)
{
    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "Alpha", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "Beta", 0.0f, 0.0f, nullptr});

    LayoutModel layout;
    layout.SetDefault({"a"});

    const auto items = BuildViewMenuItems(registry, layout);
    ASSERT_EQ(items.size(), 2u);
    // Ordered by title: Alpha, Beta.
    EXPECT_EQ(items[0].panelId, "a");
    EXPECT_EQ(items[0].title, "Alpha");
    EXPECT_TRUE(items[0].shown);
    EXPECT_EQ(items[1].panelId, "b");
    EXPECT_FALSE(items[1].shown);
}

TEST(ShellModelsTest, WindowTitleFormat)
{
    EXPECT_EQ(FormatWindowTitle("world.world", false, "Proj"), "world.world - Proj - SkyEngine Editor");
    EXPECT_EQ(FormatWindowTitle("world.world", true, "Proj"), "world.world* - Proj - SkyEngine Editor");
    EXPECT_EQ(FormatWindowTitle("", false, ""), "Untitled - SkyEngine - SkyEngine Editor");
}

TEST(ShellModelsTest, StatusTextFormat)
{
    EXPECT_EQ(FormatStatusBar("world.world", false, "Proj", "Edit", "Vulkan", 3, 60.0f),
              "world.world    Proj    Edit    RHI: Vulkan    sel: 3    60 fps");
    EXPECT_EQ(FormatStatusBar("world.world", true, "Proj", "Play", "Vulkan", 0, 30.0f),
              "world.world*    Proj    Play    RHI: Vulkan    sel: 0    30 fps");
}
