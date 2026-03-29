//
// Created by blues on 2025/5/25.
//

#include <framework/console/CommandShell.h>
#include <framework/console/CommandHistory.h>
#include <framework/console/ConsoleLog.h>
#include <core/console/CVar.h>
#include <core/console/CommandRegistry.h>
#include <core/logger/Logger.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

using namespace sky;

// ============================================================
// CommandShell tests
// ============================================================

TEST(CommandShellTest, CVarGet)
{
    CVar<int> cv("shell.TestGet", 42, "test get");
    CommandShell shell;
    auto result = shell.Execute("shell.TestGet");
    EXPECT_EQ(result.status, CommandResult::Status::OK);
    EXPECT_TRUE(result.output.find("42") != std::string::npos);
    EXPECT_TRUE(result.output.find("int") != std::string::npos);
}

TEST(CommandShellTest, CVarSet)
{
    CVar<int> cv("shell.TestSet", 10, "test set");
    CommandShell shell;
    auto result = shell.Execute("shell.TestSet 20");
    EXPECT_EQ(result.status, CommandResult::Status::OK);
    EXPECT_TRUE(result.output.find("20") != std::string::npos);
    EXPECT_TRUE(result.output.find("was: 10") != std::string::npos);
    EXPECT_EQ(cv.Get(), 20);
}

TEST(CommandShellTest, CVarReadOnlyError)
{
    CVar<int> cv("shell.TestRO", 5, "read only", CVarFlags::READ_ONLY);
    CommandShell shell;
    auto result = shell.Execute("shell.TestRO 10");
    EXPECT_EQ(result.status, CommandResult::Status::ERROR);
    EXPECT_TRUE(result.output.find("read-only") != std::string::npos);
    EXPECT_EQ(cv.Get(), 5);
}

TEST(CommandShellTest, CommandDispatch)
{
    CommandShell shell;
    auto result = shell.Execute("echo hello world");
    EXPECT_EQ(result.status, CommandResult::Status::OK);
    EXPECT_EQ(result.output, "hello world");
}

TEST(CommandShellTest, UnknownCommand)
{
    CommandShell shell;
    auto result = shell.Execute("foobar 123");
    EXPECT_EQ(result.status, CommandResult::Status::NOT_FOUND);
    EXPECT_TRUE(result.output.find("Unknown command") != std::string::npos);
}

TEST(CommandShellTest, EmptyInput)
{
    CommandShell shell;
    auto result = shell.Execute("");
    EXPECT_EQ(result.status, CommandResult::Status::OK);
    EXPECT_TRUE(result.output.empty());
}

// ============================================================
// ExecFile tests
// ============================================================

TEST(ExecFileTest, ValidFile)
{
    CVar<int> cv1("exec.A", 0, "a");
    CVar<float> cv2("exec.B", 0.0f, "b");

    // write a temp cfg file
    std::string path = "test_exec.cfg";
    {
        std::ofstream f(path);
        f << "// comment\n";
        f << "exec.A 42\n";
        f << "\n";
        f << "exec.B 3.14\n";
    }

    CommandShell shell;
    auto result = shell.ExecFile(path);
    EXPECT_EQ(result.status, CommandResult::Status::OK);
    EXPECT_TRUE(result.output.find("2 commands") != std::string::npos);
    EXPECT_EQ(cv1.Get(), 42);
    EXPECT_NEAR(cv2.Get(), 3.14f, 0.01f);

    std::filesystem::remove(path);
}

TEST(ExecFileTest, MissingFile)
{
    CommandShell shell;
    auto result = shell.ExecFile("nonexistent.cfg");
    EXPECT_EQ(result.status, CommandResult::Status::ERROR);
    EXPECT_TRUE(result.output.find("File not found") != std::string::npos);
}

// ============================================================
// CommandHistory tests
// ============================================================

TEST(CommandHistoryTest, AddAndNavigate)
{
    CommandHistory history;
    history.Add("cmd1");
    history.Add("cmd2");
    history.Add("cmd3");

    EXPECT_EQ(history.GetPrevious(), "cmd3");
    EXPECT_EQ(history.GetPrevious(), "cmd2");
    EXPECT_EQ(history.GetPrevious(), "cmd1");
    // at oldest -- stays at oldest
    EXPECT_EQ(history.GetPrevious(), "cmd1");

    EXPECT_EQ(history.GetNext(), "cmd2");
    EXPECT_EQ(history.GetNext(), "cmd3");
    // past newest -- empty
    EXPECT_TRUE(history.GetNext().empty());
}

TEST(CommandHistoryTest, ResetCursor)
{
    CommandHistory history;
    history.Add("a");
    history.Add("b");
    history.GetPrevious();
    history.ResetCursor();
    EXPECT_EQ(history.GetPrevious(), "b");
}

TEST(CommandHistoryTest, SaveLoadRoundTrip)
{
    std::string path = "test_history.txt";

    {
        CommandHistory history;
        history.Add("first");
        history.Add("second");
        history.Add("third");
        EXPECT_TRUE(history.Save(path));
    }

    {
        CommandHistory history;
        EXPECT_TRUE(history.Load(path));
        EXPECT_EQ(history.Size(), 3u);
        EXPECT_EQ(history.GetPrevious(), "third");
        EXPECT_EQ(history.GetPrevious(), "second");
        EXPECT_EQ(history.GetPrevious(), "first");
    }

    std::filesystem::remove(path);
}

TEST(CommandHistoryTest, MaxSizeTruncation)
{
    CommandHistory history;
    for (int i = 0; i < 1100; ++i) {
        history.Add("cmd" + std::to_string(i));
    }
    EXPECT_EQ(history.Size(), 1000u);
    // most recent should be cmd1099
    EXPECT_EQ(history.GetPrevious(), "cmd1099");
}

TEST(CommandHistoryTest, SkipConsecutiveDuplicates)
{
    CommandHistory history;
    history.Add("same");
    history.Add("same");
    history.Add("same");
    EXPECT_EQ(history.Size(), 1u);
}

// ============================================================
// ConsoleLog tests
// ============================================================

TEST(ConsoleLogTest, CaptureAndFlush)
{
    ConsoleLog log;
    log.Install();

    LOG_I("Test", "hello %d", 42);

    std::vector<LogEntry> entries;
    log.FlushPendingEntries([&](const LogEntry &e) {
        entries.push_back(e);
    });

    log.Uninstall();

    // find our entry (there may be entries from other logger calls)
    bool found = false;
    for (const auto &e : entries) {
        if (e.tag == "Test" && e.message == "hello 42") {
            found = true;
            EXPECT_EQ(e.level, "INFO");
        }
    }
    EXPECT_TRUE(found);
}

TEST(ConsoleLogTest, FlushOrdering)
{
    ConsoleLog log;
    log.Install();

    LOG_I("Test", "first");
    LOG_I("Test", "second");
    LOG_I("Test", "third");

    std::vector<std::string> messages;
    log.FlushPendingEntries([&](const LogEntry &e) {
        if (e.tag == "Test") {
            messages.push_back(e.message);
        }
    });

    log.Uninstall();

    ASSERT_GE(messages.size(), 3u);
    EXPECT_EQ(messages[0], "first");
    EXPECT_EQ(messages[1], "second");
    EXPECT_EQ(messages[2], "third");
}

TEST(ConsoleLogTest, DoubleFlushEmpty)
{
    ConsoleLog log;
    log.Install();

    LOG_I("Test", "msg");

    int count1 = 0;
    log.FlushPendingEntries([&](const LogEntry &) { ++count1; });

    int count2 = 0;
    log.FlushPendingEntries([&](const LogEntry &) { ++count2; });

    log.Uninstall();

    EXPECT_GT(count1, 0);
    EXPECT_EQ(count2, 0);
}
