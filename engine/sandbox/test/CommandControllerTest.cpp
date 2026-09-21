//
// Created on 2026/09/21.
//

#include <editor/core/console/CommandController.h>
#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace sky::editor;

TEST(CommandControllerTest, EmptyInputIsNoop)
{
    CommandController controller;
    std::string output;
    EXPECT_FALSE(controller.Execute("", output));
    EXPECT_TRUE(output.empty());
}

TEST(CommandControllerTest, ExecuteEchoRecordsHistory)
{
    CommandController controller;
    std::string output;
    ASSERT_TRUE(controller.Execute("echo hello", output));
    EXPECT_EQ(output, "hello");
    EXPECT_GE(controller.GetHistory().Size(), 1u);
}

TEST(CommandControllerTest, UnknownCommandFails)
{
    CommandController controller;
    std::string output;
    EXPECT_FALSE(controller.Execute("definitely_not_a_command 1", output));
    EXPECT_NE(output.find("Unknown"), std::string::npos);
}

TEST(CommandControllerTest, CompletionFindsEcho)
{
    CommandController controller;
    const auto matches = controller.Complete("ech");
    EXPECT_NE(std::find(matches.begin(), matches.end(), "echo"), matches.end());
    EXPECT_TRUE(controller.Complete("").empty());
}

TEST(CommandControllerTest, HistoryNavigation)
{
    CommandController controller;
    std::string output;
    controller.Execute("echo one", output);
    controller.Execute("echo two", output);

    EXPECT_EQ(controller.HistoryPrevious(), "echo two");
    EXPECT_EQ(controller.HistoryPrevious(), "echo one");
    EXPECT_EQ(controller.HistoryNext(), "echo two");
}
