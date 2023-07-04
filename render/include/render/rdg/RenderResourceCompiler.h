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

        using Vertex = boost::graph_traits<RenderGraph::Graph>::vertex_descriptor;
        using Graph = RenderGraph::Graph;

        void discover_vertex(Vertex u, const Graph& g);

        RenderGraph &rdg;

    protected:
        void Compile(Vertex u, RasterPass &pass);
        void Compile(Vertex u, RasterSubPass &pass);
        void Compile(Vertex u, ComputePass &pass);
        void Compile(Vertex u, CopyBlitPass &pass);

        void MountResource(Vertex u, ResourceGraph::vertex_descriptor res);
    };

} // namespace sky::rdg
