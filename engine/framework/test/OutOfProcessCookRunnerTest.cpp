//
// Created by blues on 2026/10/3.
//

#include <gtest/gtest.h>

#include <framework/asset/OutOfProcessCookRunner.h>
#include <framework/platform/PlatformBase.h>
#include <core/platform/Platform.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sky;

#if !SKY_PLATFORM_ANDROID && !SKY_PLATFORM_IOS

namespace {

    std::string StubPath(const char *mode)
    {
        std::string dir = Platform::Get()->GetBundlePath();
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
            dir += '/';
        }
#if SKY_PLATFORM_WINDOWS
        return dir + "CookWorkerStub.exe";
#else
        return dir + "CookWorkerStub";
#endif
    }

    struct Collector {
        std::mutex              mutex;
        std::condition_variable cv;
        std::vector<AssetBuildResult> results;

        void Add(const AssetBuildResult &result)
        {
            std::lock_guard<std::mutex> lock(mutex);
            results.push_back(result);
            cv.notify_all();
        }

        bool WaitCount(size_t count, int timeoutMs)
        {
            std::unique_lock<std::mutex> lock(mutex);
            return cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&]() { return results.size() >= count; });
        }

        size_t Count()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return results.size();
        }
    };

} // namespace

class OutOfProcessCookRunnerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        if (Platform::Get()->GetImpl() == nullptr) {
            Platform::Get()->Init({});
        }
    }

    void TearDown() override
    {
        // The crash/restart stub writes a sentinel; keep the working tree clean.
        std::remove("cook_stub_crashonce.flag");
    }

    static CookRunnerConfig MakeConfig(const char *mode, uint32_t timeoutMs = 4000)
    {
        CookRunnerConfig config;
        config.workerPath = StubPath(mode);
        config.extraArgs = {mode};
        config.platform = "Test";
        config.timeoutMs = timeoutMs;
        return config;
    }
};

TEST_F(OutOfProcessCookRunnerTest, HandshakeAndCook)
{
    OutOfProcessCookRunner runner(MakeConfig("ok"));
    Collector collector;
    runner.SetCompletion([&](const AssetBuildResult &result) { collector.Add(result); });

    const Uuid id = Uuid::Create();
    ASSERT_TRUE(runner.Request(CookJob{id, "common", "a.t1"}));

    ASSERT_TRUE(collector.WaitCount(1, 6000));
    EXPECT_EQ(collector.results[0].uuid, id);
    EXPECT_EQ(collector.results[0].target, "common");
    EXPECT_EQ(collector.results[0].retCode, AssetBuildRetCode::SUCCESS);

    runner.Drain();
}

TEST_F(OutOfProcessCookRunnerTest, CorrelatesMultipleRequests)
{
    OutOfProcessCookRunner runner(MakeConfig("ok"));
    Collector collector;
    runner.SetCompletion([&](const AssetBuildResult &result) { collector.Add(result); });

    const Uuid first = Uuid::Create();
    const Uuid second = Uuid::Create();
    ASSERT_TRUE(runner.Request(CookJob{first, "common", "a.t1"}));
    ASSERT_TRUE(runner.Request(CookJob{second, "common", "b.t1"}));

    ASSERT_TRUE(collector.WaitCount(2, 6000));
    bool sawFirst = false;
    bool sawSecond = false;
    for (const auto &result : collector.results) {
        EXPECT_EQ(result.retCode, AssetBuildRetCode::SUCCESS);
        sawFirst |= (result.uuid == first);
        sawSecond |= (result.uuid == second);
    }
    EXPECT_TRUE(sawFirst);
    EXPECT_TRUE(sawSecond);

    runner.Drain();
}

TEST_F(OutOfProcessCookRunnerTest, UnknownResultIdIsDropped)
{
    OutOfProcessCookRunner runner(MakeConfig("spurious"));
    Collector collector;
    runner.SetCompletion([&](const AssetBuildResult &result) { collector.Add(result); });

    // Let the stub emit its spurious (unknown-id) result first.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const Uuid id = Uuid::Create();
    ASSERT_TRUE(runner.Request(CookJob{id, "common", "a.t1"}));

    ASSERT_TRUE(collector.WaitCount(1, 6000));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(collector.Count(), 1u);
    EXPECT_EQ(collector.results[0].uuid, id);

    runner.Drain();
}

TEST_F(OutOfProcessCookRunnerTest, TimeoutFailsRequest)
{
    OutOfProcessCookRunner runner(MakeConfig("timeout", 300));
    Collector collector;
    runner.SetCompletion([&](const AssetBuildResult &result) { collector.Add(result); });

    const Uuid id = Uuid::Create();
    ASSERT_TRUE(runner.Request(CookJob{id, "common", "a.t1"}));

    ASSERT_TRUE(collector.WaitCount(1, 4000));
    EXPECT_EQ(collector.results[0].retCode, AssetBuildRetCode::FAILED);

    runner.Drain();
}

TEST_F(OutOfProcessCookRunnerTest, CrashFailsInFlightThenRestarts)
{
    const std::string sentinel = "cook_stub_crashonce.flag";
    std::remove(sentinel.c_str());

    CookRunnerConfig config = MakeConfig("crashonce", 4000);
    config.extraArgs = {"crashonce", sentinel};

    OutOfProcessCookRunner runner(config);
    Collector collector;
    runner.SetCompletion([&](const AssetBuildResult &result) { collector.Add(result); });

    const Uuid first = Uuid::Create();
    ASSERT_TRUE(runner.Request(CookJob{first, "common", "a.t1"}));
    ASSERT_TRUE(collector.WaitCount(1, 5000));
    EXPECT_EQ(collector.results[0].retCode, AssetBuildRetCode::FAILED);

    // The next request starts a fresh worker which succeeds.
    const Uuid second = Uuid::Create();
    runner.Request(CookJob{second, "common", "b.t1"});
    ASSERT_TRUE(collector.WaitCount(2, 6000));
    EXPECT_EQ(collector.results[1].retCode, AssetBuildRetCode::SUCCESS);

    runner.Drain();
}

TEST_F(OutOfProcessCookRunnerTest, HealthyWorkerAfterLogsStillCompletes)
{
    OutOfProcessCookRunner runner(MakeConfig("ok"));
    Collector collector;
    runner.SetCompletion([&](const AssetBuildResult &result) { collector.Add(result); });

    for (int i = 0; i < 3; ++i) {
        runner.Request(CookJob{Uuid::Create(), "common", "a.t1"});
    }
    ASSERT_TRUE(collector.WaitCount(3, 6000));
    EXPECT_EQ(collector.Count(), 3u);

    runner.Drain();
}

#endif // desktop
