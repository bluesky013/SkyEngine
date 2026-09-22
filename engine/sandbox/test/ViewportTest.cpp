//
// Created on 2026/09/21.
//

#include <editor/core/viewport/ViewportManager.h>
#include <gtest/gtest.h>
#include <string>

using namespace sky::editor;

TEST(ViewportManagerTest, CreateFindDestroy)
{
    ViewportManager manager;
    EXPECT_EQ(manager.GetCount(), 0u);

    ASSERT_TRUE(manager.Create(ViewportPresets::EditorViewport("viewport")));
    ASSERT_TRUE(manager.Create(ViewportPresets::Preview("preview1", "Preview")));
    EXPECT_EQ(manager.GetCount(), 2u);

    const ViewportDescriptor *editor = manager.Find("viewport");
    ASSERT_NE(editor, nullptr);
    EXPECT_EQ(editor->interaction, ViewInteraction::EDIT);
    EXPECT_EQ(editor->presentation, ViewPresentation::TEXTURE);
    EXPECT_TRUE(editor->overlays.TestBit(ViewportOverlayFlagBit::GIZMO));

    const ViewportDescriptor *preview = manager.Find("preview1");
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(preview->interaction, ViewInteraction::PREVIEW);
    EXPECT_EQ(preview->title, "Preview");

    EXPECT_TRUE(manager.Destroy("preview1"));
    EXPECT_FALSE(manager.Destroy("missing"));
    EXPECT_EQ(manager.GetCount(), 1u);
    EXPECT_EQ(manager.Find("preview1"), nullptr);
}

TEST(ViewportManagerTest, DuplicateIdReplaces)
{
    ViewportManager manager;
    ASSERT_TRUE(manager.Create(ViewportPresets::Preview("p")));
    ViewportDescriptor updated = ViewportPresets::Preview("p");
    updated.interaction = ViewInteraction::PLAY;
    ASSERT_TRUE(manager.Create(updated));
    EXPECT_EQ(manager.GetCount(), 1u);
    ASSERT_NE(manager.Find("p"), nullptr);
    EXPECT_EQ(manager.Find("p")->interaction, ViewInteraction::PLAY);
}

TEST(ViewportManagerTest, PresentationAndSize)
{
    ViewportManager manager;
    ASSERT_TRUE(manager.Create(ViewportPresets::Preview("p")));

    ASSERT_TRUE(manager.SetPresentation("p", ViewPresentation::WINDOW));
    EXPECT_EQ(manager.Find("p")->presentation, ViewPresentation::WINDOW);

    ASSERT_TRUE(manager.SetSize("p", 640, 360));
    EXPECT_EQ(manager.Find("p")->width, 640u);
    EXPECT_EQ(manager.Find("p")->height, 360u);

    EXPECT_FALSE(manager.SetPresentation("missing", ViewPresentation::WINDOW));
    EXPECT_FALSE(manager.SetSize("missing", 1, 1));
}

TEST(ViewportManagerTest, RejectEmptyIdAndDestroyAll)
{
    ViewportManager manager;
    EXPECT_FALSE(manager.Create(ViewportDescriptor{}));
    ASSERT_TRUE(manager.Create(ViewportPresets::Preview("a")));
    manager.DestroyAll();
    EXPECT_EQ(manager.GetCount(), 0u);
}
