//
// Created by blues on 2025/5/25.
//

#include <core/console/CommandToken.h>
#include <core/console/CVar.h>
#include <core/console/CommandRegistry.h>
#include <gtest/gtest.h>

using namespace sky;

// ============================================================
// CommandToken tests
// ============================================================

TEST(CommandTokenTest, SimpleTokens)
{
    auto tokens = TokenizeCommand("r.ShadowQuality 3");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], "r.ShadowQuality");
    EXPECT_EQ(tokens[1], "3");
}

TEST(CommandTokenTest, QuotedString)
{
    auto tokens = TokenizeCommand("echo \"hello world\"");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], "echo");
    EXPECT_EQ(tokens[1], "hello world");
}

TEST(CommandTokenTest, EmptyInput)
{
    auto tokens = TokenizeCommand("");
    EXPECT_TRUE(tokens.empty());
}

TEST(CommandTokenTest, WhitespaceOnly)
{
    auto tokens = TokenizeCommand("   \t  ");
    EXPECT_TRUE(tokens.empty());
}

TEST(CommandTokenTest, MultipleSpaces)
{
    auto tokens = TokenizeCommand("  a   b   c  ");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "a");
    EXPECT_EQ(tokens[1], "b");
    EXPECT_EQ(tokens[2], "c");
}

TEST(CommandTokenTest, QuotedEmpty)
{
    auto tokens = TokenizeCommand("echo \"\"");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], "echo");
    EXPECT_EQ(tokens[1], "");
}

// ============================================================
// CVar tests
// ============================================================

TEST(CVarTest, IntRegistration)
{
    {
        CVar<int> cv("test.IntVar", 42, "test int");
        EXPECT_EQ(cv.Get(), 42);
        EXPECT_EQ(cv.GetName(), "test.IntVar");
        EXPECT_EQ(cv.GetDesc(), "test int");
        EXPECT_EQ(cv.GetCategory(), "test");
        EXPECT_EQ(cv.GetTypeName(), "int");

        auto *found = CommandRegistry::Get()->FindCVar("test.IntVar");
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->ToString(), "42");
    }
    // after destruction, should be gone
    EXPECT_EQ(CommandRegistry::Get()->FindCVar("test.IntVar"), nullptr);
}

TEST(CVarTest, SetAndGet)
{
    CVar<int> cv("test.SetGet", 10, "test set/get");
    EXPECT_TRUE(cv.Set(20));
    EXPECT_EQ(cv.Get(), 20);
}

TEST(CVarTest, SetFromStringInt)
{
    CVar<int> cv("test.FromStrInt", 0, "int from string");
    EXPECT_TRUE(cv.SetFromString("123"));
    EXPECT_EQ(cv.Get(), 123);
    EXPECT_FALSE(cv.SetFromString("abc"));
    EXPECT_EQ(cv.Get(), 123);
}

TEST(CVarTest, SetFromStringBool)
{
    CVar<bool> cv("test.FromStrBool", false, "bool from string");

    EXPECT_TRUE(cv.SetFromString("true"));
    EXPECT_TRUE(cv.Get());

    EXPECT_TRUE(cv.SetFromString("0"));
    EXPECT_FALSE(cv.Get());

    EXPECT_TRUE(cv.SetFromString("on"));
    EXPECT_TRUE(cv.Get());

    EXPECT_TRUE(cv.SetFromString("OFF"));
    EXPECT_FALSE(cv.Get());

    EXPECT_TRUE(cv.SetFromString("1"));
    EXPECT_TRUE(cv.Get());

    EXPECT_FALSE(cv.SetFromString("maybe"));
    EXPECT_TRUE(cv.Get()); // unchanged
}

TEST(CVarTest, SetFromStringFloat)
{
    CVar<float> cv("test.FromStrFloat", 0.0f, "float from string");
    EXPECT_TRUE(cv.SetFromString("3.14"));
    EXPECT_NEAR(cv.Get(), 3.14f, 0.001f);
    EXPECT_FALSE(cv.SetFromString("not_a_float"));
}

TEST(CVarTest, SetFromStringDouble)
{
    CVar<double> cv("test.FromStrDouble", 0.0, "double from string");
    EXPECT_TRUE(cv.SetFromString("2.71828"));
    EXPECT_NEAR(cv.Get(), 2.71828, 0.00001);
}

TEST(CVarTest, SetFromStringString)
{
    CVar<std::string> cv("test.FromStrString", "default", "string from string");
    EXPECT_TRUE(cv.SetFromString("new value"));
    EXPECT_EQ(cv.Get(), "new value");
}

TEST(CVarTest, ReadOnlyRejectsSet)
{
    CVar<int> cv("test.ReadOnly", 5, "read only", CVarFlags::READ_ONLY);
    EXPECT_FALSE(cv.Set(10));
    EXPECT_EQ(cv.Get(), 5);
    EXPECT_FALSE(cv.SetFromString("10"));
    EXPECT_EQ(cv.Get(), 5);
}

TEST(CVarTest, OnChangeCallback)
{
    CVar<int> cv("test.OnChange", 0, "on change");
    int oldCapture = -1;
    int newCapture = -1;
    cv.onChange = [&](const int &oldVal, const int &newVal) {
        oldCapture = oldVal;
        newCapture = newVal;
    };
    cv.Set(42);
    EXPECT_EQ(oldCapture, 0);
    EXPECT_EQ(newCapture, 42);

    // same value -- no callback
    oldCapture = -1;
    newCapture = -1;
    cv.Set(42);
    EXPECT_EQ(oldCapture, -1);
    EXPECT_EQ(newCapture, -1);
}

TEST(CVarTest, CategoryParsing)
{
    CVar<int> cv1("phys.Gravity", 10, "gravity");
    EXPECT_EQ(cv1.GetCategory(), "phys");

    CVar<int> cv2("debugMode", 0, "no dot");
    EXPECT_EQ(cv2.GetCategory(), "");
}

TEST(CVarTest, ResetToDefault)
{
    CVar<int> cv("test.Reset", 100, "reset test");
    cv.Set(999);
    EXPECT_EQ(cv.Get(), 999);
    cv.ResetToDefault();
    EXPECT_EQ(cv.Get(), 100);
}

TEST(CVarTest, ToString)
{
    CVar<bool> cvBool("test.TsBool", true, "");
    EXPECT_EQ(cvBool.ToString(), "true");

    CVar<int> cvInt("test.TsInt", 42, "");
    EXPECT_EQ(cvInt.ToString(), "42");

    CVar<std::string> cvStr("test.TsStr", "hello", "");
    EXPECT_EQ(cvStr.ToString(), "hello");
}

// ============================================================
// CommandRegistry tests
// ============================================================

TEST(CommandRegistryTest, RegisterAndFindCommand)
{
    auto *registry = CommandRegistry::Get();
    registry->RegisterCommand("sys.exit", "Exit engine", "sys", [](CommandArgs) {
        return CommandResult{CommandResult::Status::OK, "exiting"};
    });

    auto *cmd = registry->FindCommand("sys.exit");
    ASSERT_NE(cmd, nullptr);
    EXPECT_EQ(cmd->name, "sys.exit");
    EXPECT_EQ(cmd->desc, "Exit engine");
    EXPECT_EQ(cmd->category, "sys");

    registry->UnregisterCommand("sys.exit");
    EXPECT_EQ(registry->FindCommand("sys.exit"), nullptr);
}

TEST(CommandRegistryTest, PrefixSearch)
{
    CVar<int> cv1("r.ShadowQuality", 2, "shadow quality");
    CVar<int> cv2("r.ShadowDistance", 100, "shadow distance");
    CVar<int> cv3("r.Wireframe", 0, "wireframe");

    auto matches = CommandRegistry::Get()->FindByPrefix("r.Shadow");
    ASSERT_EQ(matches.size(), 2u);
    // sorted alphabetically
    EXPECT_EQ(matches[0].cvar->GetName(), "r.ShadowDistance");
    EXPECT_EQ(matches[1].cvar->GetName(), "r.ShadowQuality");
}

TEST(CommandRegistryTest, HiddenExclusion)
{
    CVar<int> cv("r.HiddenVar", 0, "hidden", CVarFlags::HIDDEN);
    auto matches = CommandRegistry::Get()->FindByPrefix("r.Hidden");
    EXPECT_TRUE(matches.empty());
}

TEST(CommandRegistryTest, ForEachCVar)
{
    CVar<int> cv1("iter.A", 1, "a");
    CVar<int> cv2("iter.B", 2, "b");

    int count = 0;
    CommandRegistry::Get()->ForEachCVar([&](ICVar *cvar) {
        if (cvar->GetCategory() == "iter") {
            ++count;
        }
    });
    EXPECT_EQ(count, 2);
}
