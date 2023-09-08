//
// Created by Zach Lee on 2023/8/27.
//

#pragma once
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>

namespace sky::rdg {
    class RenderGraph;

    struct RenderSceneVisitor{
        explicit RenderSceneVisitor(RenderGraph &g) : graph(g) {}

        [[maybe_unused]] void BuildRenderQueue();

        RenderGraph &graph;
    };

} // namespace sky::rdg
