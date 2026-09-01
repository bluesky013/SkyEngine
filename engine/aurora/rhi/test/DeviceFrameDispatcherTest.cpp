//
// Created on 2026/09/01.
//

#include <gtest/gtest.h>

#include <aurora/rdg/DeviceFrameDispatcher.h>
#include <core/async/ThreadPool.h>

#include <array>
#include <atomic>

namespace sky::aurora::test {

    TEST(DeviceFrameDispatcherTest, LinearChain)
    {
        ThreadPool           pool(4);
        DeviceFrameDispatcher graph;

        std::atomic_int                counter{0};
        std::array<std::atomic_int, 3> order{};
        for (auto &o : order) {
            o.store(-1);
        }

        auto a = graph.CreateTask([&](ThreadContext &) { order[0].store(counter.fetch_add(1)); });
        auto b = graph.CreateTask([&](ThreadContext &) { order[1].store(counter.fetch_add(1)); });
        auto c = graph.CreateTask([&](ThreadContext &) { order[2].store(counter.fetch_add(1)); });

        graph.DependsOn(b, a);
        graph.DependsOn(c, b);

        graph.Submit(pool).wait();

        ASSERT_LT(order[0].load(), order[1].load());
        ASSERT_LT(order[1].load(), order[2].load());
    }

    TEST(DeviceFrameDispatcherTest, DiamondDependency)
    {
        ThreadPool           pool(4);
        DeviceFrameDispatcher graph;

        std::atomic_int                counter{0};
        std::array<std::atomic_int, 4> order{};

        auto a = graph.CreateTask([&](ThreadContext &) { order[0].store(counter.fetch_add(1)); });
        auto b = graph.CreateTask([&](ThreadContext &) { order[1].store(counter.fetch_add(1)); });
        auto c = graph.CreateTask([&](ThreadContext &) { order[2].store(counter.fetch_add(1)); });
        auto d = graph.CreateTask([&](ThreadContext &) { order[3].store(counter.fetch_add(1)); });

        graph.DependsOn(b, a);
        graph.DependsOn(c, a);
        graph.DependsOn(d, b);
        graph.DependsOn(d, c);

        graph.Submit(pool).wait();

        ASSERT_LT(order[0].load(), order[1].load());
        ASSERT_LT(order[0].load(), order[2].load());
        ASSERT_LT(order[1].load(), order[3].load());
        ASSERT_LT(order[2].load(), order[3].load());
    }

    TEST(DeviceFrameDispatcherTest, MultipleRoots)
    {
        ThreadPool           pool(4);
        DeviceFrameDispatcher graph;

        std::atomic_int                counter{0};
        std::array<std::atomic_int, 3> order{};

        auto a = graph.CreateTask([&](ThreadContext &) { order[0].store(counter.fetch_add(1)); });
        auto b = graph.CreateTask([&](ThreadContext &) { order[1].store(counter.fetch_add(1)); });
        auto c = graph.CreateTask([&](ThreadContext &) { order[2].store(counter.fetch_add(1)); });

        graph.DependsOn(c, a);
        graph.DependsOn(c, b);

        graph.Submit(pool).wait();

        ASSERT_LT(order[0].load(), order[2].load());
        ASSERT_LT(order[1].load(), order[2].load());
    }

    TEST(DeviceFrameDispatcherTest, SingleNodeNoDeps)
    {
        ThreadPool           pool(2);
        DeviceFrameDispatcher graph;

        std::atomic_int value{0};

        auto t = graph.CreateTask([&](ThreadContext &) { value.store(99, std::memory_order_relaxed); });

        graph.Submit(pool).wait();

        ASSERT_EQ(value.load(), 99);
    }

    TEST(DeviceFrameDispatcherTest, EmptyBatch)
    {
        ThreadPool           pool(2);
        DeviceFrameDispatcher graph;

        graph.Submit(pool).wait();
        ASSERT_EQ(graph.GetNodeCount(), 0u);
    }

    TEST(DeviceFrameDispatcherTest, ClearReuse)
    {
        ThreadPool           pool(2);
        DeviceFrameDispatcher graph;

        std::atomic_int first{0};
        {
            auto t = graph.CreateTask([&](ThreadContext &) { first.fetch_add(1); });
            graph.Submit(pool).wait();
        }
        ASSERT_EQ(first.load(), 1);

        graph.Clear();

        std::atomic_int second{0};
        auto            t2 = graph.CreateTask([&](ThreadContext &) { second.fetch_add(1); });
        graph.Submit(pool).wait();
        ASSERT_EQ(second.load(), 1);
    }

    TEST(DeviceFrameDispatcherTest, PerNodeFuture)
    {
        ThreadPool           pool(2);
        DeviceFrameDispatcher graph;

        std::atomic_int value{0};

        auto t = graph.CreateTask([&](ThreadContext &) { value.store(7, std::memory_order_relaxed); });
        auto f = graph.GetFuture(t);

        graph.Submit(pool).wait();
        f.wait();

        ASSERT_EQ(value.load(), 7);
    }

    TEST(DeviceFrameDispatcherTest, BatchFuture)
    {
        ThreadPool           pool(2);
        DeviceFrameDispatcher graph;

        std::atomic_int counter{0};

        for (int i = 0; i < 100; ++i) {
            graph.CreateTask([&](ThreadContext &) { counter.fetch_add(1); });
        }

        graph.Submit(pool).wait();

        ASSERT_EQ(counter.load(), 100);
    }

} // namespace sky::aurora::test
