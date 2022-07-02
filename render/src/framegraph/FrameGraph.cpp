//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraph.h>
#include <core/logger/Logger.h>

static const char* TAG = "FrameGraph";

namespace sky {

    void FrameGraph::Compile()
    {

    }

    void FrameGraph::Execute()
    {

    }

    void FrameGraph::PrintGraph()
    {
        for (auto& edge : edges) {
            LOG_I(TAG, "edge, %s -> %s", edge.from->name.c_str(), edge.to->name.c_str());
        }
    }

}