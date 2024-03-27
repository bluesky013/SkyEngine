//
// Created by Zach Lee on 2023/8/27.
//

#pragma once
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_traits.hpp>

namespace sky {
    class RenderScene;
} // namespace sky

namespace sky::rdg {
    struct RenderGraph;

    struct RenderSceneVisitor{
        explicit RenderSceneVisitor(RenderGraph &g, RenderScene *scn) : graph(g), scene(scn) {}

        [[maybe_unused]] void BuildRenderQueue();
        RenderGraph &graph;
        RenderScene *scene;
    };

} // namespace sky::rdg
