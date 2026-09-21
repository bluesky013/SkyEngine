//
// Created on 2026/09/21.
//

#include <editor/core/layout/DefaultPanels.h>
#include <editor/core/layout/PanelRegistry.h>
#include <gtest/gtest.h>

using namespace sky::editor;

TEST(DefaultPanelsTest, RegistersCorePanelIds)
{
    PanelRegistry registry;
    RegisterDefaultEditorPanels(registry);

    EXPECT_TRUE(registry.Contains("viewport"));
    EXPECT_TRUE(registry.Contains("outliner"));
    EXPECT_TRUE(registry.Contains("inspector"));
    EXPECT_TRUE(registry.Contains("outputlog"));
    EXPECT_TRUE(registry.Contains("console"));

    const PanelInfo *console = registry.Find("console");
    ASSERT_NE(console, nullptr);
    EXPECT_EQ(console->title, "Console");
    EXPECT_FLOAT_EQ(console->minWidth, 240.f);
}
