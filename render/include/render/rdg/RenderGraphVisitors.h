//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    struct RenderGraphPassCompiler : boost::dfs_visitor<> {
        RenderGraphPassCompiler(RenderGraph &g) : graph(g) {}

        using Graph = RenderGraph::Graph;
        using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
        using Edge = boost::graph_traits<Graph>::edge_descriptor;

        void tree_edge(Edge e, const Graph &g);
        void discover_vertex(Vertex u, const Graph& g);

        void Compile(RasterPass &pass);
        void Compile(ComputePass &pass);
        void Compile(CopyBlitPass &pass);

        RenderGraph &graph;
    };

}
