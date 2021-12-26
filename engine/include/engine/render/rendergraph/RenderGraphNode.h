//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <vector>

namespace sky {

    class RenderGraphNode {
    public:
        RenderGraphNode(RenderGraphNode* parent);
        virtual ~RenderGraphNode();

    private:
        RenderGraphNode* parent;
        std::vector<RenderGraphNode*> children;
    };
}