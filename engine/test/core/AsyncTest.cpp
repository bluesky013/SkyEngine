//
// Created by blues on 2024/11/24.
//
#include <core/async/NamedThread.h>
#include <core/profile/Profiler.h>
#include <gtest/gtest.h>
#include <map>

using namespace sky;

TEST(AsyncTest, NamedThreadTest)
{
    std::vector<int> v(100);
    {
        NamedThread thread;
        for (int i = 0; i < 100; ++i) {
            thread.Dispatch([&v, i]() {
                v[i] = i;
            });
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