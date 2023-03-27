//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky {

    class RenderGraphCompiler : public boost::dfs_visitor<> {
    public:
        RenderGraphCompiler() = default;
        ~RenderGraphCompiler() = default;

        using Vertex = boost::graph_traits<RenderGraph::NodeGraph>::vertex_descriptor;
        using Graph = RenderGraph::NodeGraph;

        void discover_vertex(Vertex u, const Graph& g)
        {
        }
    };

}