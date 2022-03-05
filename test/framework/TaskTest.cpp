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
    flow.Emplace([&a]() {
        a = 1;
    });

    tm->Execute(flow).wait();
    ASSERT_EQ(a, 1);

    tm->Shutdown();
    TaskManager::Destroy();
}