//
// Created by Zach Lee on 2023/5/31.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    struct ResourceGraphCompiler : boost::dfs_visitor<> {
        ResourceGraphCompiler(RenderGraph &g) : graph(g.resourceGraph) {}

        using Vertex = boost::graph_traits<ResourceGraph::Graph>::vertex_descriptor;
        using Edge = boost::graph_traits<ResourceGraph::Graph>::edge_descriptor;
        using Graph = ResourceGraph::Graph;

        void tree_edge(Edge e, const Graph &g);
        void discover_vertex(Vertex u, const Graph& g);

        ResourceGraph &graph;
    };

} // namespace sky::rdg