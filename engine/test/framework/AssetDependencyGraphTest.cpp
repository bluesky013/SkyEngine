//
// Created by blues on 2026/10/2.
//

#include <gtest/gtest.h>

#include <framework/asset/AssetDependencyProvider.h>

#include <algorithm>

using namespace sky;

TEST(AssetDependencyGraphTest, ForwardAndReverse)
{
    AssetDependencyGraph graph;

    const Uuid a = Uuid::Create();
    const Uuid b = Uuid::Create();
    const Uuid c = Uuid::Create();

    graph.Add(a, { b, c });
    graph.Add(b, { c });
    graph.Add(c, {});

    EXPECT_EQ(graph.Dependencies(a).size(), 2U);
    EXPECT_EQ(graph.Dependencies(c).size(), 0U);

    const auto dependentsOfC = graph.Dependents(c);
    EXPECT_EQ(dependentsOfC.size(), 2U);
    EXPECT_NE(std::find(dependentsOfC.begin(), dependentsOfC.end(), a), dependentsOfC.end());
    EXPECT_NE(std::find(dependentsOfC.begin(), dependentsOfC.end(), b), dependentsOfC.end());

    int count = 0;
    graph.ForEach([&count](const Uuid &, const std::vector<Uuid> &) {
        ++count;
    });
    EXPECT_EQ(count, 3);
}
