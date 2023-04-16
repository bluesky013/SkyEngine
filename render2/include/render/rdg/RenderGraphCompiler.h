//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    class RenderResourceCompiler : public boost::dfs_visitor<> {
    public:
        RenderResourceCompiler() = default;
        ~RenderResourceCompiler() = default;

        using Vertex = boost::graph_traits<ResourceGraph>::vertex_descriptor;
        using Graph = ResourceGraph;

        void discover_vertex(Vertex u, const Graph& g);
    };

    class RenderGraphCompiler : public boost::dfs_visitor<> {
    public:
        RenderGraphCompiler() = default;
        ~RenderGraphCompiler() = default;
    };

}