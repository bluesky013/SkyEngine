//
// Created by Zach Lee on 2021/12/26.
//

#pragma once
#include <string>

namespace sky {

    class RenderGraph;

    class RenderGraphBuilder {
    public:
        RenderGraphBuilder(RenderGraph& rg) : renderGraph(rg) {}
        ~RenderGraphBuilder() = default;

        template <typename T, typename Setup, typename Execute>
        void AddPass(const std::string& key, Setup&& setup, Execute&& execute)
        {
            setup(*this);
        }

    private:
        RenderGraph& renderGraph;
    };

}
