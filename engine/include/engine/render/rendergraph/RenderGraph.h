//
// Created by Zach Lee on 2021/12/22.
//

#pragma once
#include <engine/render/rendergraph/RenderGraphBuilder.h>
#include <engine/render/rendergraph/RenderGraphPass.h>
#include <string>

namespace sky {

    class RenderGraph {
    public:
        RenderGraph() = default;
        ~RenderGraph() = default;

        template <typename Data, typename Setup, typename Execute>
        void AddPass(const std::string& key, Setup&& setup, Execute&& execute)
        {
            auto pass = std::make_unique<RenderGraphPass<Data>>(key, std::forward<Execute>(execute));
            RenderGraphBuilder builder(*this, *pass);
            setup(builder, static_cast<Data&>(*pass->data));
            passes.emplace_back(std::move(pass));
        }

        void ImportImage(const std::string& str, drv::ImagePtr image);

        void Compile();

        void Execute(drv::CommandBuffer& commandBuffer);

        void Clear();

    private:
        friend class RenderGraphBuilder;
        struct Edge {
            RenderGraphNode* from;
            RenderGraphNode* to;
        };

        std::vector<Edge> edges;
        using PassPtr = std::unique_ptr<RenderGraphPassBase>;

        std::unordered_map<std::string, RGImagePtr> images;
        std::vector<PassPtr> passes;
        std::unordered_map<RenderGraphNode*, std::vector<RenderGraphNode*>> incomingEdgeMap;
    };
}