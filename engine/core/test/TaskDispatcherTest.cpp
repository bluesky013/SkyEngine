#include <core/async/TaskDispatcher.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>

using namespace sky;
using namespace std::chrono_literals;

TEST(TaskDispatcher, FailuresSkipDependentsAndDrainIndependentNodes)
{
    ThreadPool           pool(2);
    TaskDispatcher       graph;
    std::atomic_uint32_t independent   = 0;
    auto                 failed        = graph.CreateCheckedTask([](ThreadContext &) { return false; });
    auto                 skipped       = graph.CreateTask([](ThreadContext &) { ADD_FAILURE(); });
    auto                 secondFailure = graph.CreateCheckedTask([](ThreadContext &) { return false; });
    graph.CreateTask([&](ThreadContext &) { ++independent; });
    EXPECT_EQ(graph.DependsOn(skipped, failed), DispatchError::NONE);
    auto failedFuture        = graph.GetFuture(failed);
    auto skippedFuture       = graph.GetFuture(skipped);
    auto secondFailureFuture = graph.GetFuture(secondFailure);
    auto submitted           = graph.Submit(pool);
    ASSERT_TRUE(submitted.Accepted());
    ASSERT_EQ(submitted.completion.wait_for(5s), std::future_status::ready);
    EXPECT_EQ(submitted.completion.get(), DispatchState::FAILED);
    EXPECT_EQ(failedFuture.get(), DispatchState::FAILED);
    EXPECT_EQ(skippedFuture.get(), DispatchState::SKIPPED);
    EXPECT_EQ(secondFailureFuture.get(), DispatchState::FAILED);
    EXPECT_EQ(independent.load(), 1u);
}

TEST(TaskDispatcher, InvalidBuildAndCyclesAreRejectedBeforeExecution)
{
    ThreadPool     pool(1);
    TaskDispatcher graph;
    auto           a = graph.CreateTask([](ThreadContext &) { ADD_FAILURE(); });
    auto           b = graph.CreateTask([](ThreadContext &) { ADD_FAILURE(); });
    graph.DependsOn(a, b);
    graph.DependsOn(b, a);
    EXPECT_EQ(graph.Submit(pool).error, DispatchError::CYCLE);
    EXPECT_EQ(graph.Clear(), DispatchError::NONE);
    EXPECT_EQ(graph.CreateTask({}), TaskDispatcher::kInvalidNode);
    EXPECT_EQ(graph.Submit(pool).error, DispatchError::INVALID_TASK);
    graph.Clear();
    EXPECT_EQ(graph.DependsOn(1, 0), DispatchError::INVALID_EDGE);
    EXPECT_EQ(graph.Submit(pool).error, DispatchError::INVALID_EDGE);
    graph.Clear();
    EXPECT_FALSE(graph.GetFuture(7).valid());
    EXPECT_EQ(graph.GetBuildError(), DispatchError::INVALID_STATE);
    graph.Clear();
    auto node   = graph.CreateTask([](ThreadContext &) { ADD_FAILURE(); });
    auto future = graph.GetFuture(node);
    EXPECT_TRUE(future.valid());
    EXPECT_FALSE(graph.GetFuture(node).valid());
    EXPECT_EQ(graph.Submit(pool).error, DispatchError::INVALID_STATE);
}

TEST(TaskDispatcher, QueueBudgetRejectsOversizeAndSingleWorkerFanOutCompletes)
{
    ThreadPool           pool(1);
    TaskDispatcher       graph;
    std::atomic_uint32_t runs = 0;
    for (uint32_t i = 0; i < ThreadPool::GetLocalQueueCapacity(); ++i) {
        auto node = graph.CreateTask([&](ThreadContext &) { ++runs; });
        if (i)
            graph.DependsOn(node, 0);
    }
    auto submitted = graph.Submit(pool);
    ASSERT_TRUE(submitted.Accepted());
    ASSERT_EQ(submitted.completion.wait_for(5s), std::future_status::ready);
    EXPECT_EQ(submitted.completion.get(), DispatchState::SUCCEEDED);
    EXPECT_EQ(runs.load(), ThreadPool::GetLocalQueueCapacity());
    EXPECT_EQ(graph.Submit(pool).error, DispatchError::INVALID_STATE);
    graph.Clear();
    for (uint32_t i = 0; i <= ThreadPool::GetLocalQueueCapacity(); ++i)
        graph.CreateTask([](ThreadContext &) { ADD_FAILURE(); });
    EXPECT_EQ(graph.Submit(pool).error, DispatchError::BUDGET_EXCEEDED);
}

TEST(TaskDispatcher, ClearCannotRaceRunningCallbacksAndFinishedGraphCanBeReused)
{
    ThreadPool         pool(2);
    TaskDispatcher     graph;
    std::promise<void> release;
    auto               gate = release.get_future().share();
    graph.CreateTask([gate](ThreadContext &) { gate.wait_for(5s); });
    auto submitted = graph.Submit(pool);
    EXPECT_EQ(graph.Clear(), DispatchError::INVALID_STATE);
    release.set_value();
    ASSERT_TRUE(submitted.Accepted());
    EXPECT_EQ(submitted.completion.get(), DispatchState::SUCCEEDED);
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(graph.Clear(), DispatchError::NONE);
        graph.CreateTask([](ThreadContext &) {});
        auto next = graph.Submit(pool);
        ASSERT_TRUE(next.Accepted());
        EXPECT_EQ(next.completion.get(), DispatchState::SUCCEEDED);
    }
    graph.Clear();
    auto empty = graph.Submit(pool);
    ASSERT_TRUE(empty.Accepted());
    EXPECT_EQ(empty.completion.get(), DispatchState::SUCCEEDED);
}
