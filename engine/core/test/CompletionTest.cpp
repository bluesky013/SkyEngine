#include <core/async/Completion.h>
#include <core/async/TaskDispatcher.h>
#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <thread>

using namespace sky;
using namespace std::chrono_literals;

TEST(Completion, SubscriptionBeforeAndAfterPublicationNeverRunsInline)
{
    const auto             executor = ContinuationExecutor::Create(2);
    auto                   source   = std::make_shared<CompletionSource>();
    const auto             owner    = std::this_thread::get_id();
    int                    visible  = 0;
    int                    calls    = 0;
    CompletionContinuation callback = [&](const CompletionResult &result) {
        EXPECT_EQ(std::this_thread::get_id(), owner);
        EXPECT_EQ(result.state, CompletionState::SUCCEEDED);
        EXPECT_EQ(visible, 42);
        ++calls;
    };
    EXPECT_EQ(source->Subscribe(executor->Id(), callback), CompletionSubscribeResult::ACCEPTED);
    EXPECT_FALSE(callback);
    EXPECT_EQ(executor->Pump(), ContinuationPumpResult::IDLE);
    std::thread producer([&] {
        visible = 42;
        EXPECT_TRUE(source->Publish({CompletionState::SUCCEEDED}));
    });
    producer.join();
    EXPECT_EQ(calls, 0);
    CompletionContinuation late = [&](const CompletionResult &result) {
        EXPECT_EQ(result.state, CompletionState::SUCCEEDED);
        ++calls;
    };
    EXPECT_EQ(source->Subscribe(executor->Id(), late), CompletionSubscribeResult::ACCEPTED);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(executor->Pump(), ContinuationPumpResult::EXECUTED);
    EXPECT_EQ(executor->Pump(), ContinuationPumpResult::EXECUTED);
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(executor->IsDrained());
    EXPECT_FALSE(source->Publish({CompletionState::FAILED, "too late"}));
    source->RequestCancel();
    EXPECT_FALSE(source->Poll().cancelRequested);
    EXPECT_EQ(source->Poll().result.state, CompletionState::SUCCEEDED);
}

TEST(Completion, CapacityAndInvalidExecutorPreserveCallerOwnership)
{
    EXPECT_FALSE(ContinuationExecutor::Create(0));
    const auto             executor = ContinuationExecutor::Create(1);
    CompletionSource       source;
    CompletionContinuation empty;
    EXPECT_EQ(source.Subscribe(executor->Id(), empty), CompletionSubscribeResult::INVALID_CALLBACK);
    CompletionContinuation first = [](const CompletionResult &) {};
    EXPECT_EQ(source.Subscribe({executor->Id().id, 2}, first), CompletionSubscribeResult::INVALID_EXECUTOR);
    EXPECT_TRUE(first);
    EXPECT_EQ(source.Subscribe(executor->Id(), first), CompletionSubscribeResult::ACCEPTED);
    CompletionContinuation next = [](const CompletionResult &) {};
    EXPECT_EQ(source.Subscribe(executor->Id(), next), CompletionSubscribeResult::BUSY);
    EXPECT_TRUE(next);
    executor->Close();
    EXPECT_EQ(source.Subscribe(executor->Id(), next), CompletionSubscribeResult::INVALID_EXECUTOR);
    EXPECT_TRUE(next);
    EXPECT_FALSE(executor->IsDrained());
    EXPECT_FALSE(source.Publish({CompletionState::PENDING}));
    source.RequestCancel();
    EXPECT_TRUE(source.Poll().cancelRequested);
    EXPECT_EQ(source.Poll().result.state, CompletionState::PENDING);
    EXPECT_TRUE(source.Publish({CompletionState::CANCELLED}));
    EXPECT_EQ(executor->Pump(true), ContinuationPumpResult::EXECUTED);
    EXPECT_TRUE(executor->IsDrained());
    EXPECT_EQ(executor->Pump(true), ContinuationPumpResult::IDLE);
}

TEST(Completion, PublicationSubscriptionAndCancelRaceDeliverExactlyOnce)
{
    const auto executor = ContinuationExecutor::Create(2);
    for (int iteration = 0; iteration < 200; ++iteration) {
        CompletionSource       source;
        std::barrier           start(3);
        int                    calls    = 0;
        CompletionContinuation callback = [&](const CompletionResult &result) {
            EXPECT_EQ(result.state, CompletionState::SUCCEEDED);
            ++calls;
        };
        std::thread subscriber([&] {
            start.arrive_and_wait();
            EXPECT_EQ(source.Subscribe(executor->Id(), callback), CompletionSubscribeResult::ACCEPTED);
        });
        std::thread publisher([&] {
            start.arrive_and_wait();
            EXPECT_TRUE(source.Publish({CompletionState::SUCCEEDED}));
        });
        start.arrive_and_wait();
        source.RequestCancel();
        subscriber.join();
        publisher.join();
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(executor->Pump(), ContinuationPumpResult::EXECUTED);
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(executor->Pump(), ContinuationPumpResult::IDLE);
    }
}

TEST(Completion, ExecutorThreadIdentityAndReentrantPumpAreEnforced)
{
    const auto       executor = ContinuationExecutor::Create(1);
    CompletionSource source;
    std::thread      other([&] { EXPECT_EQ(executor->Pump(), ContinuationPumpResult::INVALID_THREAD); });
    other.join();
    CompletionContinuation callback = [&](const CompletionResult &) {
        EXPECT_FALSE(executor->IsDrained());
        EXPECT_EQ(executor->Pump(), ContinuationPumpResult::REENTRANT);
        CompletionContinuation nested = [](const CompletionResult &) {};
        EXPECT_EQ(source.Subscribe(executor->Id(), nested), CompletionSubscribeResult::BUSY);
        EXPECT_TRUE(nested);
        executor->Close();
    };
    EXPECT_EQ(source.Subscribe(executor->Id(), callback), CompletionSubscribeResult::ACCEPTED);
    source.Publish({CompletionState::SUCCEEDED});
    executor->Pump();
    EXPECT_TRUE(executor->IsDrained());
}

TEST(Completion, CloseKeepsCallbackAndExecutorPinsUntilCallbackDestruction)
{
    auto                                      module       = std::make_shared<int>(7);
    const std::weak_ptr<int>                  weakModule   = module;
    auto                                      executor     = ContinuationExecutor::Create(1, module);
    const auto                                id           = executor->Id();
    const std::weak_ptr<ContinuationExecutor> weakExecutor = executor;
    auto                                      captured     = std::make_shared<int>(11);
    const std::weak_ptr<int>                  weakCapture  = captured;
    CompletionSource                          source;
    CompletionContinuation                    callback = [captured, &executor, &weakModule, &weakCapture](const CompletionResult &result) {
        executor.reset();
        EXPECT_FALSE(weakModule.expired());
        EXPECT_FALSE(weakCapture.expired());
        EXPECT_EQ(result.state, CompletionState::SUCCEEDED);
    };
    EXPECT_EQ(source.Subscribe(id, callback), CompletionSubscribeResult::ACCEPTED);
    module.reset();
    captured.reset();
    executor->Close();
    source.Publish({CompletionState::SUCCEEDED});
    EXPECT_FALSE(weakCapture.expired());
    executor->Pump();
    EXPECT_TRUE(weakCapture.expired());
    EXPECT_TRUE(weakExecutor.expired());
    EXPECT_TRUE(weakModule.expired());
    CompletionContinuation rejected = [](const CompletionResult &) {};
    EXPECT_EQ(source.Subscribe(id, rejected), CompletionSubscribeResult::INVALID_EXECUTOR);
    EXPECT_TRUE(rejected);
    const auto replacement = ContinuationExecutor::Create(1);
    EXPECT_NE(replacement->Id(), id);
}

TEST(Completion, AcceptedDeliveryDoesNotDependOnWorkerQueueCapacity)
{
    const auto         executor = ContinuationExecutor::Create(1);
    CompletionSource   source;
    ThreadPool         pool(1);
    std::promise<void> release;
    std::promise<void> entered;
    auto               ready   = entered.get_future();
    const auto         gate    = release.get_future().share();
    auto               blocked = pool.Dispatch([gate, &entered](ThreadContext &) {
        entered.set_value();
        gate.wait_for(5s);
    });
    EXPECT_EQ(ready.wait_for(5s), std::future_status::ready);
    for (uint32_t i = 0; i < ThreadPool::GetLocalQueueCapacity(); ++i)
        pool.Schedule([](ThreadContext &) {});
    // This test deliberately saturates the pool without using it for completion delivery.
    int                    calls    = 0;
    CompletionContinuation callback = [&](const CompletionResult &) { ++calls; };
    EXPECT_EQ(source.Subscribe(executor->Id(), callback), CompletionSubscribeResult::ACCEPTED);
    executor->Close();
    source.Publish({CompletionState::SUCCEEDED});
    EXPECT_EQ(executor->Pump(), ContinuationPumpResult::EXECUTED);
    EXPECT_EQ(calls, 1);
    release.set_value();
    blocked.get();
    pool.WaitIdle();
}

TEST(Completion, DuplicatePublishRaceChoosesOneSharedTerminalForAllSubscribers)
{
    const auto       executor = ContinuationExecutor::Create(2);
    CompletionSource source;
    int              accepted   = 0;
    std::atomic_int  publishers = 0;
    CompletionState  delivered  = CompletionState::PENDING;
    for (int i = 0; i < 2; ++i) {
        CompletionContinuation callback = [&](const CompletionResult &result) {
            if (accepted++)
                EXPECT_EQ(result.state, delivered);
            delivered = result.state;
        };
        EXPECT_EQ(source.Subscribe(executor->Id(), callback), CompletionSubscribeResult::ACCEPTED);
    }
    std::barrier start(2);
    std::thread  failed([&] {
        start.arrive_and_wait();
        publishers += source.Publish({CompletionState::FAILED, "failure"});
    });
    start.arrive_and_wait();
    publishers += source.Publish({CompletionState::SUCCEEDED});
    failed.join();
    executor->Pump();
    executor->Pump();
    EXPECT_EQ(publishers, 1);
    EXPECT_EQ(accepted, 2);
    EXPECT_EQ(source.Wait().state, delivered);
}

TEST(TaskDispatcher, DrainHookCanReclaimGraphAndRunsBeforeJoinForEmptyAndNonemptyBatches)
{
    ThreadPool pool(2);
    for (const bool empty : {false, true}) {
        auto graph = std::make_unique<TaskDispatcher>();
        if (!empty) {
            for (int i = 0; i < 100; ++i)
                graph->CreateTask([](ThreadContext &) {});
        }
        int  calls     = 0;
        auto submitted = graph->Submit(pool, [&](DispatchState result) {
            EXPECT_EQ(result, DispatchState::SUCCEEDED);
            EXPECT_EQ(graph->Clear(), DispatchError::NONE);
            graph.reset();
            ++calls;
        });
        ASSERT_TRUE(submitted.Accepted());
        EXPECT_EQ(submitted.completion.get(), DispatchState::SUCCEEDED);
        EXPECT_FALSE(graph);
        EXPECT_EQ(calls, 1);
    }
}
