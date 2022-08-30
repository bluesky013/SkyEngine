//
// Created by Zach Lee on 2022/3/6.
//

#include <gtest/gtest.h>
#include <thread>
#include "core/jobsystem/WorkThread.h"
#include "core/logger/Logger.h"
#include "core/jobsystem/JobSystem.h"

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

    ASSERT_EQ(b,  1);

    JobSystem::Destroy();
}

class TestWorkThread : public WorkThread {
public:
    TestWorkThread() =  default;
    ~TestWorkThread() = default;

    template <typename T>
    void Setup(T&& func)
    {
        function = std::move(func);
    }

    void Execute() override
    {
        if (function) {
            function();
        }
    }

private:
    std::function<void(void)> function;
};

TEST(JobSystemTest, WorkThreadTest)
{
    int a = 1;
    TestWorkThread workThread;
    workThread.Start();
    ASSERT_EQ(a, 1);

    std::promise<void> p1;
    auto f1 = p1.get_future();
    workThread.Setup([&p1, &a]() {
        a = 2;
        p1.set_value();
    });
    workThread.Notify();
    f1.wait();
    ASSERT_EQ(a, 2);

    std::promise<void> p2;
    auto f2 = p2.get_future();
    workThread.Setup([&p2, &a]() {
        a = 3;
        p2.set_value();
    });
    workThread.Notify();
    f2.wait();
    ASSERT_EQ(a, 3);
}