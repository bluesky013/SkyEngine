//
// Created by Zach Lee on 2023/5/31.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    struct AccessCompiler : boost::dfs_visitor<> {
        AccessCompiler(RenderGraph &g) : rdg(g) {}

        using Vertex = boost::graph_traits<AccessGraph::Graph>::vertex_descriptor;
        using Edge = boost::graph_traits<AccessGraph::Graph>::edge_descriptor;
        using Graph = AccessGraph::Graph;

        void examine_edge(Edge u, const Graph& g);

        RenderGraph &rdg;
    };

} // namespace sky::rdg