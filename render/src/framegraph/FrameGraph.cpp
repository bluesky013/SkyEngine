//
// Created by Zach Lee on 2022/6/2.
//

#include <core/logger/Logger.h>
#include <render/framegraph/FrameGraph.h>

static const char *TAG = "FrameGraph";

namespace sky {

    void FrameGraph::Compile()
    {
        for (auto &pass : passes) {
            pass->Compile();
        }
    }

    void FrameGraph::Execute(drv::CommandBufferPtr commandBuffer)
    {
        for (auto &node : nodes) {
            node->Execute(commandBuffer);
        }
    }

    void FrameGraph::PrintGraph()
    {
        for (auto &edge : edges) {
            LOG_I(TAG, "edge, %s -> %s", edge.from->name.c_str(), edge.to->name.c_str());
        }
    }

} // namespace sky