//
// Created by Zach Lee on 2023/3/27.
//

#include <render/rdg/RenderGraphVisitors.h>
#include <core/logger/Logger.h>

static const char *TAG = "RDG";

namespace sky::rdg {

    void RenderGraphPassCompiler::discover_vertex(Vertex u, const Graph& g) {
        LOG_I(TAG, "compile passes %s....", Name(u, graph).c_str());
        std::visit(Overloaded{
            [&](const ComputePassTag &) {
                auto &compute = graph.computePasses[Index(u, graph)];
            },
            [&](const CopyBlitTag &) {
                auto &cb = graph.copyBlitPasses[Index(u, graph)];
            },
            [&](const PresentTag &) {
                auto &present = graph.presentPasses[Index(u, graph)];
            },
            [&](const auto &) {}
        }, Tag(u, graph));
    }
}
