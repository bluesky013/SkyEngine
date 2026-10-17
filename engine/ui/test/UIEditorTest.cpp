//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/editor/UIDocumentEditor.h>

namespace sky::ui {

    TEST(UIEditorTest, AddAndRoundTrip)
    {
        UIDocumentEditor editor;
        UIElement *child = editor.AddChild("", "Image");
        ASSERT_NE(child, nullptr);
        child->SetName("Pic");
        const std::string json = editor.Serialize();

        UIDocumentEditor reloaded;
        ASSERT_TRUE(reloaded.Deserialize(json));
        UIElement *found = reloaded.Find("Pic");
        ASSERT_NE(found, nullptr);
        EXPECT_STREQ(found->GetTypeName(), "Image");
    }

    TEST(UIEditorTest, RemoveAndUndoRedo)
    {
        UIDocumentEditor editor;
        editor.AddChild("", "Image")->SetName("Pic");
        ASSERT_NE(editor.Find("Pic"), nullptr);

        ASSERT_TRUE(editor.Remove("Pic"));
        EXPECT_EQ(editor.Find("Pic"), nullptr);
        EXPECT_TRUE(editor.CanUndo());

        ASSERT_TRUE(editor.Undo());
        EXPECT_NE(editor.Find("Pic"), nullptr);

        ASSERT_TRUE(editor.Redo());
        EXPECT_EQ(editor.Find("Pic"), nullptr);
    }

    TEST(UIEditorTest, MoveReordersSiblings)
    {
        UIDocumentEditor editor;
        editor.AddChild("", "Image")->SetName("A");
        editor.AddChild("", "Image")->SetName("B");

        UIElement *root = editor.GetRoot();
        ASSERT_EQ(root->GetChildren().size(), 2u);
        EXPECT_EQ(root->GetChildren()[0]->GetName(), "A");

        ASSERT_TRUE(editor.MoveUp("B"));
        EXPECT_EQ(root->GetChildren()[0]->GetName(), "B");
        EXPECT_EQ(root->GetChildren()[1]->GetName(), "A");
    }

} // namespace sky::ui
