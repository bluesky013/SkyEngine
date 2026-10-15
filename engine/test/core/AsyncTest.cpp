//
// Created by blues on 2024/11/24.
//
#include <core/async/NamedThread.h>
#include <core/profile/Profiler.h>
#include <future>
#include <gtest/gtest.h>
#include <map>
#include <memory>

using namespace sky;

TEST(AsyncTest, DestructionDrainsQueuedSignalBeforeDestroyingSemaphore)
{
    auto               thread = std::make_unique<NamedThread>();
    std::promise<void> entered;
    std::promise<void> release;
    auto               ready = entered.get_future();
    auto               gate  = release.get_future().share();
    thread->Dispatch([&entered, gate]() {
        entered.set_value();
        gate.wait();
    });
    thread->Signal();
    ready.wait();
    std::promise<void> destructionStarted;
    auto               started   = destructionStarted.get_future();
    auto               destroyed = std::async(std::launch::async, [owned = std::move(thread), &destructionStarted]() mutable {
        destructionStarted.set_value();
        owned.reset();
    });
    started.wait();
    EXPECT_EQ(destroyed.wait_for(std::chrono::milliseconds(30)), std::future_status::timeout);
    release.set_value();
    EXPECT_EQ(destroyed.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    destroyed.get();
}

TEST(AsyncTest, NamedThreadTest)
{
    std::vector<int> v(100);
    {
        NamedThread thread;
        for (int i = 0; i < 100; ++i) {
            thread.Dispatch([&v, i]() { v[i] = i; });
        }
    }
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(v[i], i);
    }
}

TEST(AsyncTest, FrameTest)
{
    NamedThread thread;

    struct FrameData {
        int a;
        int b;
    };
    FrameData data = {0, 0};

    {
        SKY_FRAME_MARK("test1")
        // frame 1
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(5)); // payload

        thread.Sync();
        thread.Dispatch([&data]() {
            data.a = 1;
            data.b = 2;
            std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(16.6));
        });
        thread.Signal();
    }

    {
        SKY_FRAME_MARK("test2")
        // frame 2
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(2)); // payload

        thread.Sync();
        ASSERT_EQ(data.a, 1);
        ASSERT_EQ(data.b, 2);

        thread.Dispatch([&data]() {
            data.a = 3;
            data.b = 4;
            std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(16.6));
        });
        thread.Signal();
    }

    {
        SKY_FRAME_MARK("test3")
        // frame 3
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(3)); // payload

        thread.Sync();
        ASSERT_EQ(data.a, 3);
        ASSERT_EQ(data.b, 4);

        thread.Dispatch([&data]() {
            data.a = 5;
            data.b = 6;
            std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(16.6));
        });
        thread.Signal();
    }
}
