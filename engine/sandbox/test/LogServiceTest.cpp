//
// Created on 2026/09/21.
//

#include <core/logger/Logger.h>
#include <editor/core/log/LogService.h>
#include <gtest/gtest.h>
#include <string>

using namespace sky;
using namespace sky::editor;

TEST(LogServiceTest, CaptureAndFilter)
{
    LogService log;
    log.Install();
    LOG_I("EditorTest", "hello world");
    LOG_E("EditorTest", "something failed");
    log.Uninstall();
    log.Pump();

    log.SetTagFilter("EditorTest");
    size_t tagged = 0;
    for (const auto &entry : log.GetVisibleEntries()) {
        EXPECT_EQ(entry.tag, "EditorTest");
        ++tagged;
    }
    EXPECT_GE(tagged, 2u);

    log.SetLevelFilter("ERROR");
    ASSERT_FALSE(log.GetVisibleEntries().empty());
    for (const auto &entry : log.GetVisibleEntries()) {
        EXPECT_EQ(entry.level, "ERROR");
    }

    log.ResetFilters();
    log.SetTagFilter("EditorTest");
    log.SetSearch("failed");
    for (const auto &entry : log.GetVisibleEntries()) {
        EXPECT_EQ(entry.tag, "EditorTest");
        EXPECT_NE(entry.message.find("failed"), std::string::npos);
    }
}

TEST(LogServiceTest, ClearRemovesEntries)
{
    LogService log;
    log.Install();
    LOG_I("ClearTest", "x");
    log.Uninstall();
    log.Pump();

    ASSERT_GT(log.GetTotalCount(), 0u);
    log.Clear();
    EXPECT_EQ(log.GetTotalCount(), 0u);
    EXPECT_TRUE(log.GetVisibleEntries().empty());
}

TEST(LogServiceTest, CapacityIsBounded)
{
    LogService log;
    log.Install();
    for (size_t i = 0; i < LogService::kCapacity + 64; ++i) {
        LOG_I("CapTest", "entry");
    }
    log.Uninstall();
    log.Pump();

    EXPECT_LE(log.GetTotalCount(), LogService::kCapacity);
}

TEST(LogServiceTest, InstallUninstallState)
{
    LogService log;
    EXPECT_FALSE(log.IsInstalled());
    log.Install();
    EXPECT_TRUE(log.IsInstalled());
    log.Uninstall();
    EXPECT_FALSE(log.IsInstalled());
}
