//
// Created by blues on 2026/10/3.
//

#include <gtest/gtest.h>

#include <framework/platform/Process.h>
#include <framework/platform/PlatformBase.h>
#include <core/platform/Platform.h>

#include <chrono>
#include <cstdio>
#include <string>

using namespace sky;

#if !SKY_PLATFORM_ANDROID && !SKY_PLATFORM_IOS

namespace {

    std::string HelperPath()
    {
        std::string dir = Platform::Get()->GetBundlePath();
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
            dir += '/';
        }
#if SKY_PLATFORM_WINDOWS
        return dir + "ProcessHelper.exe";
#else
        return dir + "ProcessHelper";
#endif
    }

    std::string ReadUntil(IProcess *process, bool errorStream, const std::string &needle, int timeoutMs)
    {
        std::string out;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            uint8_t buffer[256];
            bool    eof = false;
            const size_t count = errorStream ? process->ReadStderr(buffer, sizeof(buffer), eof)
                                             : process->ReadStdout(buffer, sizeof(buffer), eof);
            if (count > 0) {
                out.append(reinterpret_cast<const char *>(buffer), count);
                if (out.find(needle) != std::string::npos) {
                    break;
                }
            }
            if (eof) {
                break;
            }
        }
        return out;
    }

} // namespace

class ProcessTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        // GetBundlePath() needs the platform backend; init once for the whole suite.
        if (Platform::Get()->GetImpl() == nullptr) {
            Platform::Get()->Init({});
        }
    }
};

TEST_F(ProcessTest, EchoStdinStdoutAndStderr)
{
    auto process = CreateProcess();
    ASSERT_NE(process, nullptr);

    ProcessDesc desc;
    desc.args = {HelperPath(), "echo"};
    ASSERT_TRUE(process->Start(desc));

    EXPECT_NE(ReadUntil(process.get(), true, "ready", 3000).find("ready"), std::string::npos);

    ASSERT_TRUE(process->WriteStdin(reinterpret_cast<const uint8_t *>("hello"), 5));
    EXPECT_NE(ReadUntil(process.get(), false, "hello", 3000).find("hello"), std::string::npos);

    process->Kill();
}

TEST_F(ProcessTest, ExitCodeReported)
{
    auto process = CreateProcess();
    ASSERT_NE(process, nullptr);

    ProcessDesc desc;
    desc.args = {HelperPath(), "exit", "7"};
    ASSERT_TRUE(process->Start(desc));

    int exitCode = 0;
    EXPECT_TRUE(process->WaitFor(5000, exitCode));
    EXPECT_EQ(exitCode, 7);
}

TEST_F(ProcessTest, WaitTimesOutThenKill)
{
    auto process = CreateProcess();
    ASSERT_NE(process, nullptr);

    ProcessDesc desc;
    desc.args = {HelperPath(), "sleep"};
    ASSERT_TRUE(process->Start(desc));

    int exitCode = 0;
    EXPECT_FALSE(process->WaitFor(200, exitCode));

    process->Kill();
    EXPECT_TRUE(process->WaitFor(5000, exitCode));
    EXPECT_FALSE(process->IsRunning());
}

TEST_F(ProcessTest, MissingExecutableFailsToStart)
{
    auto process = CreateProcess();
    ASSERT_NE(process, nullptr);

    ProcessDesc desc;
    desc.args = {HelperPath() + "_does_not_exist", "echo"};
    EXPECT_FALSE(process->Start(desc));
    EXPECT_EQ(process->GetStatus(), ProcessStatus::NotFound);
}

#endif // desktop
