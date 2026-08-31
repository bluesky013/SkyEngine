//
// Created on 2026/09/01.
//

#include <gtest/gtest.h>

#include <aurora/rdg/FrameGraphDispatcher.h>
#include <core/async/ThreadPool.h>

#include <chrono>
#include <cstdio>
#include <vector>

namespace sky::aurora::test {

    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    namespace {

        double ToMs(TimePoint t0, TimePoint t1)
        {
            return std::chrono::duration<double, std::milli>(t1 - t0).count();
        }

        uint32_t RunTaskNodeGraph(ThreadPool &pool, uint32_t layers, uint32_t nodesPerLayer)
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

        uint32_t RunDispatcherGraph(FrameGraphDispatcher &graph, ThreadPool &pool, uint32_t layers, uint32_t nodesPerLayer)
        {
            std::vector<std::vector<FrameGraphDispatcher::NodeIndex>> ids(layers);
            for (uint32_t l = 0; l < layers; ++l) {
                ids[l].reserve(nodesPerLayer);
                for (uint32_t i = 0; i < nodesPerLayer; ++i) {
                    ids[l].emplace_back(graph.CreateTask([](ThreadContext &) {}));
                }
            }

            for (uint32_t l = 1; l < layers; ++l) {
                for (uint32_t i = 0; i < nodesPerLayer; ++i) {
                    graph.DependsOn(ids[l][i], ids[l - 1][i]);
                    if (i + 1 < nodesPerLayer) {
                        graph.DependsOn(ids[l][i], ids[l - 1][i + 1]);
                    }
                }
            }

            graph.Submit(pool).wait();
            return layers * nodesPerLayer;
        }

    } // namespace

    TEST(FrameGraphDispatcherBenchmark, DependencyGraphCompare)
    {
        constexpr uint32_t LAYERS = 32;
        constexpr uint32_t NODES  = 64;
        constexpr int      ITERS  = 50;

        {
            ThreadPool pool(8);
            auto       t0    = Clock::now();
            uint32_t   total = 0;
            for (int it = 0; it < ITERS; ++it) {
                total += RunTaskNodeGraph(pool, LAYERS, NODES);
            }
            auto t1 = Clock::now();
            std::printf("[Benchmark] TaskNode  %ux%u x%d: %.2f ms (%.2f M nodes/s)\n", LAYERS, NODES, ITERS, ToMs(t0, t1),
                        total / ToMs(t0, t1) / 1000.0);
        }

        {
            ThreadPool           pool(8);
            FrameGraphDispatcher graph;
            auto                 t0    = Clock::now();
            uint32_t             total = 0;
            for (int it = 0; it < ITERS; ++it) {
                total += RunDispatcherGraph(graph, pool, LAYERS, NODES);
                graph.Clear();
            }
            auto t1 = Clock::now();
            std::printf("[Benchmark] Dispatcher %ux%u x%d: %.2f ms (%.2f M nodes/s)\n", LAYERS, NODES, ITERS, ToMs(t0, t1),
                        total / ToMs(t0, t1) / 1000.0);
        }
    }

} // namespace sky::aurora::test
