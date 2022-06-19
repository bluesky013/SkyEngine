//
// Created by Zach Lee on 2022/3/6.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <framework/jobsystem/JobSystem.h>

using namespace sky;


TEST(JobSystemTest, TaskFlowTest)
{
    auto js = JobSystem::Get();

    int a = 0;
    tf::Taskflow flow;
    auto t1 = flow.emplace([&a]() {
        a = 1;
    });
    auto t2 = flow.emplace([&a]() {
        a++;
    });
    t2.succeed(t1);

    js->RunAndWait(flow);
    ASSERT_EQ(a, 2);

    JobSystem::Destroy();
}