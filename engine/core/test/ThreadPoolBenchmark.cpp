//
// Created on 2026/09/01.
//

#include <core/async/ThreadPool.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <vector>

using namespace sky;

using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

TEST(ThreadPoolBenchmark, TypeSizes)
{
    std::printf("[Sizeof] RefObject=%zu TaskNode=%zu ThreadTask=%zu SmallVector4=%zu SpinLock=%zu\n", sizeof(RefObject), sizeof(TaskNode),
                sizeof(ThreadTask), sizeof(SmallVector<CounterPtr<TaskNode>, 4>), sizeof(SpinLock));
}

namespace {

    double ToMs(TimePoint t0, TimePoint t1)
    {
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    uint32_t RunDependencyGraph(ThreadPool &pool, uint32_t layers, uint32_t nodesPerLayer)
    {
        std::vector<std::vector<TaskNodePtr>> graph(layers);
        for (uint32_t l = 0; l < layers; ++l) {
            graph[l].reserve(nodesPerLayer);
            for (uint32_t i = 0; i < nodesPerLayer; ++i) {
                graph[l].emplace_back(pool.CreateTask([](ThreadContext &) {}));
            }
        }

        for (uint32_t l = 1; l < layers; ++l) {
            for (uint32_t i = 0; i < nodesPerLayer; ++i) {
                graph[l][i]->DependsOn(graph[l - 1][i]);
                if (i + 1 < nodesPerLayer) {
                    graph[l][i]->DependsOn(graph[l - 1][i + 1]);
                }
            }
        }

        for (uint32_t i = 0; i < nodesPerLayer; ++i) {
            pool.Submit(graph[0][i]);
        }
        pool.WaitIdle();
        return layers * nodesPerLayer;
    }

} // namespace

TEST(ThreadPoolBenchmark, DispatchThroughput)
{
    ThreadPool    pool(8);
    constexpr int N = 200000;

    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) {
        pool.Dispatch([](ThreadContext &) {});
    }
    pool.WaitIdle();
    auto t1 = Clock::now();

    const double ms = ToMs(t0, t1);
    std::printf("[Benchmark] Dispatch x%d: %.2f ms (%.2f M tasks/s)\n", N, ms, N / ms / 1000.0);
}

TEST(ThreadPoolBenchmark, DependencyGraphThroughput)
{
    ThreadPool         pool(8);
    constexpr uint32_t LAYERS = 32;
    constexpr uint32_t NODES  = 64;
    constexpr int      ITERS  = 50;

    constexpr uint32_t nodesPerIter = LAYERS * NODES;
    uint32_t           totalNodes   = 0;

    auto t0 = Clock::now();
    for (int it = 0; it < ITERS; ++it) {
        totalNodes += RunDependencyGraph(pool, LAYERS, NODES);
    }
    auto t1 = Clock::now();

    const double ms = ToMs(t0, t1);
    std::printf("[Benchmark] DependencyGraph %ux%u x%d: %.2f ms (%.2f M nodes/s)\n", LAYERS, NODES, ITERS, ms, totalNodes / ms / 1000.0);
}
