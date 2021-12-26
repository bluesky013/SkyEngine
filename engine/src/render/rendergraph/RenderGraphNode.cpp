//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphNode.h>

namespace sky {

    static void Erase(std::vector<RenderGraphNode*>& nodes, RenderGraphNode* node)
    {
        auto iter = std::find(nodes.begin(), nodes.end(), node);
        if (iter != nodes.end()) {
            nodes.erase(iter);
        }
    }

    static void Emplace(std::vector<RenderGraphNode*>& nodes, RenderGraphNode* node)
    {
        auto iter = std::find(nodes.begin(), nodes.end(), node);
        if (iter == nodes.end()) {
            nodes.emplace_back(node);
        }
    }

    RenderGraphNode::RenderGraphNode(RenderGraphNode* p) : parent(p)
    {
        if (parent != nullptr) {
            Emplace(parent->children, this);
        }
    }

    RenderGraphNode::~RenderGraphNode()
    {
        if (parent != nullptr) {
            Erase(parent->children, this);
        }
        for (auto& child : children) {
            child->parent = nullptr;
            delete child;
        }
        children.clear();
    }

}