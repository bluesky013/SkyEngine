//
// Created by Zach Lee on 2023/8/27.
//

#pragma once
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {
    class RenderScene;

    struct RenderSceneVisitor{
        explicit RenderSceneVisitor(RenderGraph &g) : graph(g) {}

        using Graph = RenderGraph::Graph;
        using Vertex = boost::graph_traits<Graph>::vertex_descriptor;

        [[maybe_unused]] void BuildRenderQueue();

        RenderGraph &graph;
    };

} // namespace sky::rdg
