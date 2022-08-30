//
// Created by Zach Lee on 2022/3/6.
//

#include "core/jobsystem/JobSystem.h"
#include "core/logger/Logger.h"
#include <gtest/gtest.h>
#include <thread>

using namespace sky;

TEST(JobSystemTest, TaskFlowTest)
{
    auto js = JobSystem::Get();

    int          a = 0;
    tf::Taskflow flow;
    auto         t1 = flow.emplace([&a]() { a = 1; });
    auto         t2 = flow.emplace([&a]() { a++; });
    t2.succeed(t1);

    js->RunAndWait(flow);
    ASSERT_EQ(a, 2);

    JobSystem::Destroy();
}

TEST(JobSystemTest, TaskHandleTest)
{
    auto js = JobSystem::Get();

    tf::Taskflow flow1;

    int a = 0;
    int b = 0;

    flow1.emplace([&a]() {
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(50));
        a++;
    });
    auto future = js->Run(flow1);

    tf::Taskflow flow2;
    flow1.emplace([&a, &b, &future]() {
        b = a;
        future.wait();
    });
    js->RunAndWait(flow1);

    ASSERT_EQ(b, 1);

    JobSystem::Destroy();
}