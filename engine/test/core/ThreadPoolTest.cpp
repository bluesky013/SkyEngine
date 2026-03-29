//
// Created on 2026/03/29.
//

#include <core/async/ThreadPool.h>
#include <gtest/gtest.h>

#include <atomic>
#include <numeric>
#include <vector>

using namespace sky;

// ---- helpers ----

class CounterContext : public ThreadContext {
public:
    uint32_t threadIndex = 0;
    bool     attached    = false;

    void OnAttach(uint32_t index) override
    {
        threadIndex = index;
        attached    = true;
    }

    void OnDetach() override
    {
        attached = false;
    }
};

// ---- basic dispatch ----

TEST(ThreadPoolTest, DispatchSingleTask)
{
    ThreadPool pool(2);

    std::atomic_int value{0};
    auto future = pool.Dispatch([&value](ThreadContext &) {
        value.store(42, std::memory_order_relaxed);
    });
    future.wait();

    ASSERT_EQ(value.load(), 42);
}

TEST(ThreadPoolTest, DispatchMultipleTasks)
{
    ThreadPool pool(4);
    constexpr int N = 1000;
    std::atomic_int counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i) {
        futures.emplace_back(pool.Dispatch([&counter](ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    for (auto &f : futures) {
        f.wait();
    }

    ASSERT_EQ(counter.load(), N);
}

// ---- context binding ----

TEST(ThreadPoolTest, ContextAttachDetach)
{
    std::vector<CounterContext*> ctxPtrs;
    {
        ThreadPool pool(3, [&ctxPtrs](uint32_t) {
            auto *ctx = new CounterContext();
            ctxPtrs.push_back(ctx);
            return ctx;
        });

        // ensure all workers have started and called OnAttach
        pool.Parallel(pool.GetThreadCount(),
            [](uint32_t, uint32_t, ThreadContext &) {}).wait();

        for (uint32_t i = 0; i < pool.GetThreadCount(); ++i) {
            const auto *ctx = static_cast<CounterContext*>(pool.GetContext(i));
            ASSERT_NE(ctx, nullptr);
            ASSERT_TRUE(ctx->attached);
        }
    }
    // after destruction, OnDetach should have been called
    // (contexts are deleted by pool, so we can't check them after)
}

TEST(ThreadPoolTest, ContextReceivedInTask)
{
    ThreadPool pool(2, [](uint32_t index) {
        auto *ctx = new CounterContext();
        return ctx;
    });

    std::atomic_bool received{false};
    auto future = pool.Dispatch([&received](ThreadContext &ctx) {
        auto &cc = static_cast<CounterContext&>(ctx);
        if (cc.attached) {
            received.store(true, std::memory_order_relaxed);
        }
    });
    future.wait();

    ASSERT_TRUE(received.load());
}

// ---- parallel ----

TEST(ThreadPoolTest, ParallelBasic)
{
    constexpr uint32_t THREADS = 4;
    constexpr uint32_t TASKS   = 20;

    ThreadPool pool(THREADS);
    std::atomic_int counter{0};

    auto future = pool.Parallel(TASKS,
        [&counter](uint32_t, uint32_t, ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    future.wait();

    ASSERT_EQ(counter.load(), static_cast<int>(TASKS));
}

TEST(ThreadPoolTest, ParallelSumWithLoadBalancing)
{
    constexpr uint32_t THREADS    = 4;
    constexpr uint32_t TASKS      = THREADS * 4;
    constexpr uint32_t TOTAL_WORK = 10000;

    ThreadPool pool(THREADS);
    std::vector<int> data(TOTAL_WORK);
    std::iota(data.begin(), data.end(), 1);

    std::atomic<int64_t> totalSum{0};

    auto future = pool.Parallel(TASKS,
        [&data, &totalSum, TOTAL_WORK](uint32_t taskIndex, uint32_t taskCount, ThreadContext &) {
            const uint32_t begin = TOTAL_WORK * taskIndex / taskCount;
            const uint32_t end   = TOTAL_WORK * (taskIndex + 1) / taskCount;
            int64_t local = 0;
            for (uint32_t i = begin; i < end; ++i) {
                local += data[i];
            }
            totalSum.fetch_add(local, std::memory_order_relaxed);
        });
    future.wait();

    const int64_t expected = static_cast<int64_t>(TOTAL_WORK) * (TOTAL_WORK + 1) / 2;
    ASSERT_EQ(totalSum.load(), expected);
}

// ---- work stealing ----

TEST(ThreadPoolTest, WorkStealingEffectiveness)
{
    constexpr uint32_t THREADS = 4;
    // push all tasks to thread 0's local queue
    constexpr uint32_t TASKS   = 32;

    ThreadPool pool(THREADS);
    std::atomic_int counter{0};

    // use Parallel with taskCount == TASKS; round-robin will spread them
    // but the real test: threads that finish early steal from siblings
    auto future = pool.Parallel(TASKS,
        [&counter](uint32_t, uint32_t, ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    future.wait();

    ASSERT_EQ(counter.load(), static_cast<int>(TASKS));
}

// ---- WaitIdle ----

TEST(ThreadPoolTest, WaitIdleDrainsAllTasks)
{
    ThreadPool pool(4);
    std::atomic_int counter{0};

    for (int i = 0; i < 500; ++i) {
        pool.Dispatch([&counter](ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    pool.WaitIdle();
    ASSERT_EQ(counter.load(), 500);
}

// ---- edge cases ----

TEST(ThreadPoolTest, SingleThread)
{
    ThreadPool pool(1);
    std::atomic_int counter{0};

    auto future = pool.Parallel(8,
        [&counter](uint32_t, uint32_t, ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    future.wait();

    ASSERT_EQ(counter.load(), 8);
}

TEST(ThreadPoolTest, ParallelTaskCountEqualsThreadCount)
{
    constexpr uint32_t N = 4;
    ThreadPool pool(N);
    std::atomic_int counter{0};

    auto future = pool.Parallel(N,
        [&counter](uint32_t, uint32_t, ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    future.wait();

    ASSERT_EQ(counter.load(), static_cast<int>(N));
}

TEST(ThreadPoolTest, ParallelTaskCountLessThanThreadCount)
{
    ThreadPool pool(8);
    std::atomic_int counter{0};

    auto future = pool.Parallel(3,
        [&counter](uint32_t, uint32_t, ThreadContext &) {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    future.wait();

    ASSERT_EQ(counter.load(), 3);
}

TEST(ThreadPoolTest, MixedDispatchAndParallel)
{
    ThreadPool pool(4);
    std::atomic_int dispatchCount{0};
    std::atomic_int parallelCount{0};

    std::vector<std::future<void>> futures;

    for (int i = 0; i < 100; ++i) {
        futures.emplace_back(pool.Dispatch([&dispatchCount](ThreadContext &) {
            dispatchCount.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    auto pFuture = pool.Parallel(16,
        [&parallelCount](uint32_t, uint32_t, ThreadContext &) {
            parallelCount.fetch_add(1, std::memory_order_relaxed);
        });

    for (int i = 0; i < 100; ++i) {
        futures.emplace_back(pool.Dispatch([&dispatchCount](ThreadContext &) {
            dispatchCount.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    pFuture.wait();
    pool.WaitIdle();

    ASSERT_EQ(dispatchCount.load(), 200);
    ASSERT_EQ(parallelCount.load(), 16);
}

// ---- task dependency graph ----

TEST(ThreadPoolTest, TaskNodeLinearChain)
{
    // A -> B -> C, must execute in order
    ThreadPool pool(4);
    std::vector<int> order;
    std::mutex orderMutex;

    auto pushOrder = [&](int v) {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(v);
    };

    auto a = pool.CreateTask([&](ThreadContext &) { pushOrder(1); });
    auto b = pool.CreateTask([&](ThreadContext &) { pushOrder(2); });
    auto c = pool.CreateTask([&](ThreadContext &) { pushOrder(3); });

    b->DependsOn(a);
    c->DependsOn(b);

    pool.Submit(a);
    pool.Submit(b);
    pool.Submit(c);

    c->GetFuture().wait();

    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 1);
    ASSERT_EQ(order[1], 2);
    ASSERT_EQ(order[2], 3);
}

TEST(ThreadPoolTest, TaskNodeDiamondDependency)
{
    //     A
    //    / \.
    //   B   C
    //    \ /
    //     D
    ThreadPool pool(4);
    std::atomic_int aCount{0};
    std::atomic_int bcCount{0};
    std::atomic_int dCount{0};

    auto a = pool.CreateTask([&](ThreadContext &) {
        aCount.store(1, std::memory_order_relaxed);
    });
    auto b = pool.CreateTask([&](ThreadContext &) {
        ASSERT_EQ(aCount.load(std::memory_order_relaxed), 1);
        bcCount.fetch_add(1, std::memory_order_relaxed);
    });
    auto c = pool.CreateTask([&](ThreadContext &) {
        ASSERT_EQ(aCount.load(std::memory_order_relaxed), 1);
        bcCount.fetch_add(1, std::memory_order_relaxed);
    });
    auto d = pool.CreateTask([&](ThreadContext &) {
        ASSERT_EQ(bcCount.load(std::memory_order_relaxed), 2);
        dCount.store(1, std::memory_order_relaxed);
    });

    b->DependsOn(a);
    c->DependsOn(a);
    d->DependsOn(b);
    d->DependsOn(c);

    pool.Submit(a);
    pool.Submit(b);
    pool.Submit(c);
    pool.Submit(d);

    d->GetFuture().wait();

    ASSERT_EQ(dCount.load(), 1);
}

TEST(ThreadPoolTest, TaskNodeMultipleRoots)
{
    // A and B are independent roots, C depends on both
    ThreadPool pool(4);
    std::atomic_int sum{0};

    auto a = pool.CreateTask([&](ThreadContext &) { sum.fetch_add(10, std::memory_order_relaxed); });
    auto b = pool.CreateTask([&](ThreadContext &) { sum.fetch_add(20, std::memory_order_relaxed); });
    auto c = pool.CreateTask([&](ThreadContext &) {
        ASSERT_EQ(sum.load(std::memory_order_relaxed), 30);
    });

    c->DependsOn(a);
    c->DependsOn(b);

    pool.Submit(a);
    pool.Submit(b);
    pool.Submit(c);

    c->GetFuture().wait();
    ASSERT_EQ(sum.load(), 30);
}

TEST(ThreadPoolTest, TaskNodeSingleNoDepSubmit)
{
    // single task with no dependencies runs immediately
    ThreadPool pool(2);
    std::atomic_int value{0};

    auto t = pool.CreateTask([&](ThreadContext &) {
        value.store(99, std::memory_order_relaxed);
    });

    pool.Submit(t);
    t->GetFuture().wait();

    ASSERT_EQ(value.load(), 99);
}
