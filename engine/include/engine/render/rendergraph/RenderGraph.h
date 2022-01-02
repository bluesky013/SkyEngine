//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <engine/render/rendergraph/RenderGraphBuilder.h>
#include <engine/render/rendergraph/RenderGraphPass.h>
#include <engine/render/rendergraph/RenderGraphDatabase.h>
#include <string>

namespace sky {

    class RenderGraph {
    public:
        RenderGraph() = default;
        ~RenderGraph() = default;

        template <typename Data, typename Setup, typename Execute>
        void AddPass(const std::string& key, Setup&& setup, Execute&& execute)
        {
            auto pass = new RenderGraphPass<Data>(key, std::forward<Execute>(execute));
            RenderGraphBuilder builder(*this, *pass);
            setup(builder, static_cast<Data&>(*pass->data));
        }

        void Compile();

        void Clear();

    private:
        friend class RenderGraphBuilder;
        struct Edge {
            RenderGraphNode* from;
            RenderGraphNode* to;
        };

        RenderGraphDatabase database;
        std::vector<Edge> edges;
        std::unordered_map<std::string, RenderGraphResource*> resources;
        std::unordered_map<std::string, RenderGraphPassBase*> passes;
        std::unordered_map<RenderGraphNode*, std::vector<RenderGraphNode*>> incomingEdgeMap;
    };


}