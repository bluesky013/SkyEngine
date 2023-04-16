//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    struct RenderResourceCompiler : boost::dfs_visitor<> {
        using Vertex = boost::graph_traits<ResourceGraph>::vertex_descriptor;
        using Graph = ResourceGraph;

        void discover_vertex(Vertex u, const Graph& g);
    };

    struct RenderGraphCompiler : boost::dfs_visitor<> {
        RenderGraphCompiler() = default;
        ~RenderGraphCompiler() = default;
    };

}