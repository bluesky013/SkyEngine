//
// Created on 2026/10/23.
//

#include <core/tree/Bvh.h>
#include <gtest/gtest.h>
#include <random>

namespace sky {
    struct RayTestBounds {
        AABB bounds;
    };
    template <>
    struct BvhTraits<RayTestBounds> {
        static AABB GetBounds(const RayTestBounds &value)
        {
            return value.bounds;
        }
    };
} // namespace sky
using namespace sky;

TEST(BvhRay, ParallelAxisOnZeroThicknessBoundaryDoesNotClipRay)
{
    Bvh<RayTestBounds> bvh;
    bvh.Build({{{{0, 0, 0}, {0, 1, 1}}}});
    int hits = 0;
    bvh.QueryRay({0, .5f, 5}, {0, 0, -1}, 10, [&](const auto &) { ++hits; });
    EXPECT_EQ(hits, 1);
    bvh.QueryRay({.1f, .5f, 5}, {0, 0, -1}, 10, [&](const auto &) { ++hits; });
    EXPECT_EQ(hits, 1);
}

TEST(BvhRay, IdenticalBoundsHaveBoundedDepthAndCancellationClearsPartialTree)
{
    Bvh<RayTestBounds>         bvh;
    std::vector<RayTestBounds> input(4000, {{{0, 0, 0}, {1, 1, 1}}});
    bvh.Build(input);
    int hits = 0;
    bvh.QueryRay({.5f, .5f, 5}, {0, 0, -1}, 10, [&](const auto &) { ++hits; });
    EXPECT_EQ(hits, 4000);
    std::atomic<bool> cancel{true};
    bvh.Build(input, &cancel);
    EXPECT_TRUE(bvh.Empty());
    EXPECT_EQ(bvh.GetNodeCount(), 0u);
}

TEST(BvhRay, BinnedBuilderMatchesExhaustiveBoundsAcrossRandomRays)
{
    std::mt19937                          random(8301);
    std::uniform_real_distribution<float> position(-20.f, 20.f);
    std::vector<RayTestBounds>            input;
    for (uint32_t i = 0; i < 4096; ++i) {
        const Vector3 point{position(random), position(random), position(random)};
        input.push_back({{point, point + Vector3{.3f}}});
    }
    Bvh<RayTestBounds> bvh;
    bvh.Build(input);
    for (int ray = 0; ray < 256; ++ray) {
        const Vector3 origin{position(random), position(random), 40};
        uint32_t      expected = 0, actual = 0;
        for (const auto &entry : input)
            expected += origin.x >= entry.bounds.min.x && origin.x <= entry.bounds.max.x && origin.y >= entry.bounds.min.y &&
                        origin.y <= entry.bounds.max.y && entry.bounds.max.z >= -40 && entry.bounds.min.z <= 40;
        bvh.QueryRay(origin, {0, 0, -1}, 80, [&](const auto &) { ++actual; });
        EXPECT_EQ(actual, expected);
    }
    EXPECT_LE(bvh.GetBuildStatistics().scratchBytes, Bvh<RayTestBounds>::BuildScratchBytes(input.size()));
}

TEST(BvhRay, SuspendedTraversalEqualsFullTraversalWithoutDuplicateCallbacks)
{
    std::vector<RayTestBounds> input(4000, {{{0, 0, 0}, {1, 1, 1}}});
    Bvh<RayTestBounds>         tree;
    tree.Build(input);
    Bvh<RayTestBounds>::RayCursor cursor;
    uint32_t                      visits = 0, batches = 0;
    bool                          complete = false;
    while (!complete && batches++ < 10000) {
        uint32_t work = 3;
        complete      = tree.QueryRayStep({.5f, .5f, 5}, {0, 0, -1}, 10, cursor, work, [&](const auto &) { ++visits; });
    }
    EXPECT_TRUE(complete);
    EXPECT_EQ(visits, input.size());
    EXPECT_GT(batches, 1u);
}
namespace sky {
    struct CancelBounds {
        AABB               bounds;
        std::atomic<bool> *cancel;
        uint32_t          *remaining;
    };
    template <>
    struct BvhTraits<CancelBounds> {
        static AABB GetBounds(const CancelBounds &value)
        {
            if (--*value.remaining == 0)
                value.cancel->store(true);
            return value.bounds;
        }
    };
} // namespace sky
TEST(BvhRay, CancelDuringPrimitivePreparationNeverPublishesPartialTree)
{
    std::atomic<bool>         cancel{false};
    uint32_t                  remaining = 50;
    std::vector<CancelBounds> input(4096, {{{0, 0, 0}, {1, 1, 1}}, &cancel, &remaining});
    Bvh<CancelBounds>         tree;
    tree.Build(input, &cancel);
    EXPECT_TRUE(tree.Empty());
    EXPECT_EQ(tree.GetNodeCount(), 0u);
    cancel.store(false);
    remaining = 100000;
    tree.Build(input, &cancel);
    EXPECT_EQ(tree.GetElementCount(), input.size());
}
