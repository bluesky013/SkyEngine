//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {
    struct RenderGraph;

    struct AccessCompiler : boost::dfs_visitor<> {
        AccessCompiler(RenderGraph &g) : rdg(g) {}

        using Vertex = boost::graph_traits<AccessGraph::Graph>::vertex_descriptor;
        using Edge = boost::graph_traits<AccessGraph::Graph>::edge_descriptor;
        using Graph = AccessGraph::Graph;

        void examine_edge(Edge u, const Graph& g);

        RenderGraph &rdg;
    };

    struct ResourceGraphCompiler : boost::dfs_visitor<> {
        ResourceGraphCompiler(RenderGraph &g) : graph(g.resourceGraph) {}

        using Vertex = boost::graph_traits<ResourceGraph::Graph>::vertex_descriptor;
        using Edge = boost::graph_traits<ResourceGraph::Graph>::edge_descriptor;
        using Graph = ResourceGraph::Graph;

        void tree_edge(Edge e, const Graph &g);
        void discover_vertex(Vertex u, const Graph& g);

        ResourceGraph &graph;
    };

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
