//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {
    struct RenderGraph;

    struct LifeTimeVisitor : boost::dfs_visitor<> {
        LifeTimeVisitor(RenderGraph &g) : graph(g.resourceGraph) {}

        using Vertex = boost::graph_traits<ResourceGraph::Graph>::vertex_descriptor;
        using Edge = boost::graph_traits<ResourceGraph::Graph>::edge_descriptor;
        using Graph = ResourceGraph::Graph;

        void tree_edge(Edge e, const Graph &g);

        ResourceGraph &graph;
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

        using Vertex = boost::graph_traits<PassGraph>::vertex_descriptor;
        using Edge = boost::graph_traits<PassGraph>::edge_descriptor;
        using Graph = PassGraph;

        void tree_edge(Edge e, const Graph &g);
        void discover_vertex(Vertex u, const Graph& g);

        void Compile(RasterPass &pass);
        void Compile(ComputePass &pass);
        void Compile(CopyBlitPass &pass);

        RenderGraph &graph;
    };

    struct RenderDependencyCompiler : boost::dfs_visitor<> {
        RenderDependencyCompiler(RenderGraph &g) : graph(g) {}

        using Vertex = boost::graph_traits<PassGraph>::vertex_descriptor;
        using Edge = boost::graph_traits<PassGraph>::edge_descriptor;
        using Graph = PassGraph;

        void examine_edge(Edge u, const Graph& g);
        RenderGraph &graph;
    };

}
