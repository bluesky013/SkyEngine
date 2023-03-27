//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraph.h>
#include <render/rdg/RenderGraphCompiler.h>

namespace sky {

    void RenderGraph::Compile()
    {
        RenderGraphCompiler compiler;

        auto v1 = boost::add_vertex(nodes);
        auto v2 = boost::add_vertex(nodes);

        boost::add_edge(v1, v2, nodes);

        boost::depth_first_search(nodes, boost::visitor(compiler));
    }

}