//
// Created by Zach Lee on 2023/3/27.
//

#pragma once

#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    struct RenderGraphExecutor : boost::dfs_visitor<> {
        explicit RenderGraphExecutor(RenderGraph &g) : graph(g) {}

        using Graph = RenderGraph::Graph;
        using Vertex = boost::graph_traits<Graph>::vertex_descriptor;

        [[maybe_unused]] void discover_vertex(Vertex u, const Graph& g);
        [[maybe_unused]] void finish_vertex(Vertex u, const Graph& g);

        void Barriers(const PmrHashMap<VertexType, GraphBarrier>& barriers) const;

        RenderGraph &graph;
        std::shared_ptr<rhi::GraphicsEncoder> currentEncoder;
        uint32_t currentSubPassIndex = 0;
        uint32_t currentSubPassNum = 1;
    };

}