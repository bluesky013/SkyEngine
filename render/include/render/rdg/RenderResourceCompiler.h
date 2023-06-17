//
// Created by Zach Lee on 2023/5/31.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    struct RenderResourceCompiler : boost::dfs_visitor<> {
        RenderResourceCompiler(RenderGraph &g) : rdg(g) {}

        using Vertex = boost::graph_traits<AccessGraph::Graph>::vertex_descriptor;
        using Graph = AccessGraph::Graph;

        void discover_vertex(Vertex u, const Graph& g);

        RenderGraph &rdg;

    protected:
        void MountResource(Vertex u, ResourceGraph::vertex_descriptor res);
    };

} // namespace sky::rdg