//
// Created by blues on 2024/6/2.
//

#include <gtest/gtest.h>

#include <core/std/Graph.h>

#include <algorithm>

using namespace sky;

namespace {

    int IndexOf(const std::vector<Graph::Vertex> &order, Graph::Vertex vtx)
    {
        return static_cast<int>(std::find(order.begin(), order.end(), vtx) - order.begin());
    }

} // namespace

TEST(GraphTest, EmptyGraph)
{
    Graph graph;
    const auto order = graph.TopologicalSort();
    EXPECT_TRUE(order.empty());
    EXPECT_FALSE(graph.IsCyclic());
}

TEST(GraphTest, DependenciesFirst)
{
    Graph graph;
    const auto a = graph.AddVertex();
    const auto b = graph.AddVertex();
    const auto c = graph.AddVertex(); // a depends on b, b depends on c
    graph.AddEdge(a, b);
    graph.AddEdge(b, c);

    const auto order = graph.TopologicalSort();
    ASSERT_EQ(order.size(), 3U);
    EXPECT_LT(IndexOf(order, c), IndexOf(order, b));
    EXPECT_LT(IndexOf(order, b), IndexOf(order, a));
    EXPECT_FALSE(graph.IsCyclic());
}

TEST(GraphTest, Diamond)
{
    Graph graph;
    const auto root = graph.AddVertex();
    const auto left = graph.AddVertex();
    const auto right = graph.AddVertex();
    const auto leaf = graph.AddVertex();
    graph.AddEdge(root, left);
    graph.AddEdge(root, right);
    graph.AddEdge(left, leaf);
    graph.AddEdge(right, leaf);

    const auto order = graph.TopologicalSort();
    ASSERT_EQ(order.size(), 4U);
    EXPECT_LT(IndexOf(order, leaf), IndexOf(order, left));
    EXPECT_LT(IndexOf(order, leaf), IndexOf(order, right));
    EXPECT_LT(IndexOf(order, left), IndexOf(order, root));
    EXPECT_LT(IndexOf(order, right), IndexOf(order, root));
}

TEST(GraphTest, DuplicateEdges)
{
    Graph graph;
    const auto a = graph.AddVertex();
    const auto b = graph.AddVertex();
    graph.AddEdge(a, b);
    graph.AddEdge(a, b);

    EXPECT_EQ(graph.Edges().size(), 1U);
    const auto order = graph.TopologicalSort();
    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], b);
    EXPECT_EQ(order[1], a);
}

TEST(GraphTest, CycleDetected)
{
    Graph graph;
    const auto a = graph.AddVertex();
    const auto b = graph.AddVertex();
    graph.AddEdge(a, b);
    graph.AddEdge(b, a);

    const auto order = graph.TopologicalSort();
    EXPECT_TRUE(graph.IsCyclic());
    EXPECT_EQ(order.size(), 2U);
}
