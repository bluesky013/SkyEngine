//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {
    struct RenderGraph;

    struct RenderResourceCompiler : boost::dfs_visitor<> {
        RenderResourceCompiler(RenderGraph &g) : graph(g) {}

        using Vertex = boost::graph_traits<ResourceGraph>::vertex_descriptor;
        using Edge = boost::graph_traits<ResourceGraph>::edge_descriptor;
        using Graph = ResourceGraph;

        void tree_edge(Edge e, const Graph &g);
        void discover_vertex(Vertex u, const Graph& g);

        RenderGraph &graph;
    };

    struct RenderGraphCompiler : boost::dfs_visitor<> {
        RenderGraphCompiler() = default;
        ~RenderGraphCompiler() = default;
    };

}