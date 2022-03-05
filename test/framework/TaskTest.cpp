//
// Created by Zach Lee on 2022/3/6.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <framework/task/TaskManager.h>

using namespace sky;


TEST(TaskManagerTest, TaskTest)
{
    auto tm = TaskManager::Get();
    tm->Setup();

    int a = 0;
    TaskFlow flow;
    auto t1 = flow.Emplace([&a]() {
        a = 1;
    });
    auto t2 = flow.Emplace([&a]() {
        a++;
    });
    t2.succeed(t1);

    tm->Execute(flow).wait();
    ASSERT_EQ(a, 2);

    tm->Shutdown();
    TaskManager::Destroy();
}