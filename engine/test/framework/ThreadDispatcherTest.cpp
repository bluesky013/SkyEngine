//
// ThreadDispatcher: per-role cross-thread callback queues (see editor-cook-async).
//

#include <core/thread/ThreadDispatcher.h>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace sky;

TEST(ThreadDispatcherTest, PostFromWorkerRunsOnDispatchThread)
{
    auto *dispatcher = ThreadDispatcher::Get();

    std::atomic<std::thread::id> ranOn{};
    std::atomic<bool>            ran{false};

    std::thread worker([&]() {
        dispatcher->Post(ThreadRole::Main, [&]() {
            ranOn = std::this_thread::get_id();
            ran   = true;
        });
    });
    worker.join();

    // Posted but not run until the owner dispatches.
    EXPECT_FALSE(ran.load());

    const std::thread::id owner = std::this_thread::get_id();
    dispatcher->Dispatch(ThreadRole::Main);

    EXPECT_TRUE(ran.load());
    EXPECT_EQ(ranOn.load(), owner);

    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, ReentrantPostRunsOnNextDispatch)
{
    auto *dispatcher = ThreadDispatcher::Get();

    int order  = 0;
    int first  = 0;
    int second = 0;
    dispatcher->Post(ThreadRole::Main, [&]() {
        first = ++order;
        // A callback posting again must not recurse; it lands in the next dispatch.
        dispatcher->PostToMain([&]() { second = ++order; });
    });

    dispatcher->Dispatch(ThreadRole::Main);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 0);

    dispatcher->Dispatch(ThreadRole::Main);
    EXPECT_EQ(second, 2);

    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, CallbacksRunInFifoOrder)
{
    auto *dispatcher = ThreadDispatcher::Get();

    std::vector<int> order;
    dispatcher->Post(ThreadRole::Main, [&]() { order.push_back(1); });
    dispatcher->Post(ThreadRole::Main, [&]() { order.push_back(2); });
    dispatcher->Post(ThreadRole::Main, [&]() { order.push_back(3); });

    dispatcher->Dispatch(ThreadRole::Main);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, RolesAreIndependent)
{
    auto *dispatcher = ThreadDispatcher::Get();

    int mainRuns   = 0;
    int renderRuns = 0;
    dispatcher->Post(ThreadRole::Main, [&]() { ++mainRuns; });
    dispatcher->Post(ThreadRole::Render, [&]() { ++renderRuns; });

    dispatcher->Dispatch(ThreadRole::Render);
    EXPECT_EQ(renderRuns, 1);
    EXPECT_EQ(mainRuns, 0); // the Main queue is untouched by a Render dispatch

    dispatcher->Dispatch(ThreadRole::Main);
    EXPECT_EQ(mainRuns, 1);

    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, ClearDropsPendingCallbacks)
{
    auto *dispatcher = ThreadDispatcher::Get();

    int runs = 0;
    dispatcher->Post(ThreadRole::Main, [&]() { ++runs; });
    dispatcher->Clear();
    dispatcher->Dispatch(ThreadRole::Main);
    EXPECT_EQ(runs, 0);

    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, PostAfterClearRunsOnNextDispatch)
{
    auto *dispatcher = ThreadDispatcher::Get();

    int dropped = 0;
    int kept    = 0;
    dispatcher->Post(ThreadRole::Main, [&]() { ++dropped; });
    dispatcher->Clear();
    dispatcher->Post(ThreadRole::Main, [&]() { ++kept; });

    dispatcher->Dispatch(ThreadRole::Main);
    EXPECT_EQ(dropped, 0);
    EXPECT_EQ(kept, 1);

    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, ConcurrentPostsFromManyWorkersAllRun)
{
    auto *dispatcher = ThreadDispatcher::Get();

    constexpr int kThreads   = 8;
    constexpr int kPerThread = 200;

    const std::thread::id owner = std::this_thread::get_id();
    std::atomic<int>      ran{0};
    std::atomic<int>      ranOffOwner{0};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&]() {
            for (int i = 0; i < kPerThread; ++i) {
                dispatcher->Post(ThreadRole::Main, [&]() {
                    if (std::this_thread::get_id() != owner) {
                        ranOffOwner.fetch_add(1);
                    }
                    ran.fetch_add(1);
                });
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    // All posts happened-before the join; a single dispatch runs them all on this thread.
    dispatcher->Dispatch(ThreadRole::Main);

    EXPECT_EQ(ran.load(), kThreads * kPerThread);
    EXPECT_EQ(ranOffOwner.load(), 0);

    dispatcher->Clear();
}

TEST(ThreadDispatcherTest, EmptyDispatchIsNoOp)
{
    auto *dispatcher = ThreadDispatcher::Get();

    // No pending callbacks for any role: dispatching must be a harmless no-op.
    dispatcher->Dispatch(ThreadRole::Main);
    dispatcher->Dispatch(ThreadRole::Render);
    dispatcher->Dispatch(ThreadRole::Device);

    SUCCEED();
}
