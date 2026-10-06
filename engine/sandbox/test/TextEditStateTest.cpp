//
// Created on 2026/10/06.
//

#include <editor/core/text/TextEditState.h>

#include <gtest/gtest.h>
#include <string>

using namespace sky::editor;

namespace {
    constexpr std::uint32_t kVkBack   = 0x08;
    constexpr std::uint32_t kVkEnd    = 0x23;
    constexpr std::uint32_t kVkHome   = 0x24;
    constexpr std::uint32_t kVkLeft   = 0x25;
    constexpr std::uint32_t kVkRight  = 0x27;
    constexpr std::uint32_t kVkDelete = 0x2E;
    constexpr std::uint32_t kVkA      = 0x41;
} // namespace

TEST(TextEditStateTest, SetTextPlacesCaretAtEnd)
{
    TextEditState state;
    state.SetText("MyProject");
    EXPECT_EQ(state.GetText(), "MyProject");
    EXPECT_EQ(state.GetCaret(), 9u);
    EXPECT_FALSE(state.HasSelection());
}

TEST(TextEditStateTest, InsertAndFilterControlCharacters)
{
    TextEditState state;
    state.SetText("abc");
    state.SetCaret(1);

    EXPECT_TRUE(state.OnText("X"));
    EXPECT_EQ(state.GetText(), "aXbc");
    EXPECT_EQ(state.GetCaret(), 2u);

    // Control characters (Backspace/Enter/Tab/Esc) are rejected.
    EXPECT_FALSE(state.OnText(std::string(1, '\x08')));
    EXPECT_FALSE(state.OnText(std::string(1, '\x0D')));
    EXPECT_FALSE(state.OnText(std::string(1, '\x7F')));
    EXPECT_EQ(state.GetText(), "aXbc");
}

TEST(TextEditStateTest, BackspaceAndDelete)
{
    TextEditState state;
    state.SetText("abc"); // caret at end

    state.EraseBackward();
    EXPECT_EQ(state.GetText(), "ab");
    EXPECT_EQ(state.GetCaret(), 2u);

    // Delete at the end does nothing (standard forward delete).
    state.EraseForward();
    EXPECT_EQ(state.GetText(), "ab");

    state.SetCaret(0);
    state.EraseForward();
    EXPECT_EQ(state.GetText(), "b");
    EXPECT_EQ(state.GetCaret(), 0u);
}

TEST(TextEditStateTest, SelectAllAndReplace)
{
    TextEditState state;
    state.SetText("MyProject");
    state.SelectAll();
    EXPECT_TRUE(state.HasSelection());
    EXPECT_EQ(state.GetSelectionStart(), 0u);
    EXPECT_EQ(state.GetSelectionEnd(), 9u);

    state.Insert("New");
    EXPECT_EQ(state.GetText(), "New");
    EXPECT_EQ(state.GetCaret(), 3u);
    EXPECT_FALSE(state.HasSelection());
}

TEST(TextEditStateTest, EraseDeletesSelection)
{
    TextEditState state;
    state.SetText("abcdef");
    state.SetCaret(1);
    state.MoveRight(true); // selection [1,2)
    state.MoveRight(true); // selection [1,3)
    EXPECT_TRUE(state.HasSelection());
    state.EraseBackward();
    EXPECT_EQ(state.GetText(), "adef");
    EXPECT_EQ(state.GetCaret(), 1u);
}

TEST(TextEditStateTest, MovementAndCollapse)
{
    TextEditState state;
    state.SetText("abcd"); // caret 4

    state.MoveHome(false);
    EXPECT_EQ(state.GetCaret(), 0u);
    state.MoveRight(false);
    EXPECT_EQ(state.GetCaret(), 1u);
    state.MoveEnd(false);
    EXPECT_EQ(state.GetCaret(), 4u);
    state.MoveLeft(false);
    EXPECT_EQ(state.GetCaret(), 3u);

    // Shift extends the selection.
    state.MoveHome(false);
    state.MoveRight(true);
    state.MoveRight(true);
    EXPECT_TRUE(state.HasSelection());
    EXPECT_EQ(state.GetSelectionStart(), 0u);
    EXPECT_EQ(state.GetSelectionEnd(), 2u);
}

TEST(TextEditStateTest, OnKeyUsesVirtualKeyCodes)
{
    TextEditState state;
    state.SetText("abc");

    EXPECT_TRUE(state.OnKey(kVkBack, false, false));
    EXPECT_EQ(state.GetText(), "ab");
    EXPECT_TRUE(state.OnKey(kVkHome, false, false));
    EXPECT_EQ(state.GetCaret(), 0u);
    EXPECT_TRUE(state.OnKey(kVkDelete, false, false));
    EXPECT_EQ(state.GetText(), "b");
    EXPECT_TRUE(state.OnKey(kVkA, false, true)); // Ctrl+A
    EXPECT_TRUE(state.HasSelection());
    EXPECT_TRUE(state.OnKey(kVkRight, false, false)); // collapse to end
    EXPECT_FALSE(state.HasSelection());
    EXPECT_EQ(state.GetCaret(), 1u);
    EXPECT_FALSE(state.OnKey(0x30, false, false)); // '0' is not handled here
}
