//
// Created by blues on 2026/10/2.
//

#include <gtest/gtest.h>

#include <framework/asset/AssetExecutor.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace sky;

TEST(AssetExecutorTest, DependentOrderAndWait)
{
    std::atomic<int> firstDone{0};
    std::atomic<int> secondDone{0};

    auto first = AssetExecutor::Get()->DependentAsync([&firstDone]() {
        firstDone.store(1);
    });

    auto second = AssetExecutor::Get()->DependentAsync([&firstDone, &secondDone]() {
        EXPECT_EQ(firstDone.load(), 1);
        secondDone.store(1);
    }, std::vector<TaskNodePtr>{ first.first });

    second.second.wait();
    EXPECT_EQ(secondDone.load(), 1);

    AssetExecutor::Get()->WaitForAll();
}

TEST(AssetExecutorTest, WaitForAllAfterTasks)
{
    std::atomic<int> count{0};
    for (int i = 0; i < 16; ++i) {
        AssetExecutor::Get()->DependentAsync([&count]() {
            count.fetch_add(1);
        });
    }

    AssetExecutor::Get()->WaitForAll();
    EXPECT_EQ(count.load(), 16);
}

TEST(AssetExecutorTest, DependentOnCompletedParent)
{
    std::atomic<int> ran{0};

    auto first = AssetExecutor::Get()->DependentAsync([&ran]() {
        ran.store(1);
    });
    first.second.wait();

    // Parent is already complete when the edge is declared.
    auto second = AssetExecutor::Get()->DependentAsync([&ran]() {
        ran.store(2);
    }, std::vector<TaskNodePtr>{ first.first });

    second.second.wait();
    EXPECT_EQ(ran.load(), 2);

    AssetExecutor::Get()->WaitForAll();
}

TEST(AssetExecutorTest, MixedCompletedAndPendingParents)
{
    std::atomic<int> doneFirst{0};
    std::atomic<int> ran{0};

    auto completed = AssetExecutor::Get()->DependentAsync([&doneFirst]() {
        doneFirst.store(1);
    });
    completed.second.wait();

    auto pending = AssetExecutor::Get()->DependentAsync([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    });

    auto dependent = AssetExecutor::Get()->DependentAsync([&ran]() {
        EXPECT_EQ(1, 1);
        ran.store(1);
    }, std::vector<TaskNodePtr>{ completed.first, pending.first });

    dependent.second.wait();
    EXPECT_EQ(ran.load(), 1);

    AssetExecutor::Get()->WaitForAll();
}
