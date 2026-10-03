//
// Created by blues on 2026/10/3.
//

#include <core/logger/Logger.h>
#include <gtest/gtest.h>
#include <cstdio>
#include <string>

using namespace sky;

namespace {

    std::string ReadAll(FILE *file)
    {
        std::fflush(file);
        std::rewind(file);

        std::string out;
        char        buffer[256];
        size_t      count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
            out.append(buffer, count);
        }
        return out;
    }

} // namespace

class LoggerOutputTest : public ::testing::Test {
protected:
    void TearDown() override
    {
        Logger::SetOutputStream(nullptr);
        Logger::SetOutputCallback(nullptr);
    }
};

TEST_F(LoggerOutputTest, RedirectsToConfiguredStream)
{
    FILE *file = std::tmpfile();
    ASSERT_NE(nullptr, file);

    Logger::SetOutputStream(file);
    LOG_I("LoggerOutputTest", "redirect %d", 7);

    const std::string content = ReadAll(file);
    EXPECT_NE(std::string::npos, content.find("[LoggerOutputTest] [INFO] : redirect 7"));

    std::fclose(file);
}

TEST_F(LoggerOutputTest, CallbackStillFiresWhenRedirected)
{
    FILE *file = std::tmpfile();
    ASSERT_NE(nullptr, file);

    Logger::SetOutputStream(file);

    std::string seen;
    Logger::SetOutputCallback([&seen](const char *tag, const char *type, const char *message) {
        seen = std::string(tag) + "|" + type + "|" + message;
    });

    LOG_W("Tag", "warn %d", 3);
    EXPECT_EQ("Tag|WARNING|warn 3", seen);

    std::fclose(file);
}

TEST_F(LoggerOutputTest, ResetToDefaultIsSafe)
{
    FILE *file = std::tmpfile();
    ASSERT_NE(nullptr, file);

    Logger::SetOutputStream(file);
    Logger::SetOutputStream(nullptr);
    LOG_I("LoggerOutputTest", "back to default");

    SUCCEED();
    std::fclose(file);
}
