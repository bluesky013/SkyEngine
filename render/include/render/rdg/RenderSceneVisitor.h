//
// Created by Zach Lee on 2023/8/27.
//

#pragma once
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {
    class RenderScene;

    struct RenderSceneVisitor : boost::dfs_visitor<> {
        explicit RenderSceneVisitor(RenderGraph &g) : graph(g) {}

        using Graph = RenderGraph::Graph;
        using Vertex = boost::graph_traits<Graph>::vertex_descriptor;

        [[maybe_unused]] void discover_vertex(Vertex u, const Graph& g);
//        [[maybe_unused]] void finish_vertex(Vertex u, const Graph& g);

        RenderGraph &graph;
    };

} // namespace sky::rdg