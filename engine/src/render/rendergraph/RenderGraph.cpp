//
// Created by Zach Lee on 2021/12/22.
//

#include <engine/render/rendergraph/RenderGraph.h>

namespace sky {

    void RenderGraph::Clear()
    {
        incomingEdgeMap.clear();
        images.clear();
        images.clear();
        passes.clear();
        edges.clear();
    }

    void RenderGraph::Compile()
    {
//        for (auto& edge : edges) {
//            edge.from->AddRef();
//        }
//        std::vector<RenderGraphNode*> stack;
//        for (auto& res : images) {
//            if (!res.second->IsActive()) stack.emplace_back(res.second.get());
//        }
//        for (auto& pass : passes) {
//            if (!pass->IsActive()) stack.emplace_back(pass.get());
//        }
//
//        while (!stack.empty()) {
//            RenderGraphNode* node = stack.back();
//            stack.pop_back();
//            auto& vector = incomingEdgeMap[node];
//            for (auto& in : vector) {
//                in->RemoveRef();
//                if (!in->IsActive()) stack.emplace_back(in);
//            }
//        }

        for (auto& image : images) {
            image.second->BuildResource();
        }
    }

    void RenderGraph::Execute(drv::CommandBuffer& commandBuffer)
    {
        for (auto& pass : passes) {
            pass->Execute(*this, commandBuffer);
        }
    }

}