//
// Created by Zach Lee on 2022/1/2.
//

#pragma once

#include <engine/render/rendergraph/RenderGraphNode.h>
#include <engine/render/rendergraph/RenderGraphPassData.h>
#include <vulkan/CommandBuffer.h>
#include <functional>
#include <memory>

namespace sky {
    class RenderGraph;
    class RenderGraphBuilder;

    class RenderGraphPassBase : public RenderGraphNode {
    public:
        RenderGraphPassBase(std::string&& str) : RenderGraphNode(std::forward<std::string>(str)) {}
        ~RenderGraphPassBase() = default;
    };

    template <typename Data>
    class RenderGraphPass : public RenderGraphPassBase {
    public:
        using ExecuteType = std::function<void(const Data &, const RenderGraph&, drv::CommandBuffer&)>;

        RenderGraphPass(std::string str, ExecuteType&& exe)
            : RenderGraphPassBase(std::move(str))
            , execute(exe)
            , data(std::make_unique<Data>())
        {
        }

        ~RenderGraphPass() = default;

        void Execute(const RenderGraph& graph, drv::CommandBuffer& cmd)
        {
            if (execute) execute(static_cast<Data&>(*data), graph, cmd);
        }

    private:
        friend class RenderGraph;
        ExecuteType execute;
        std::unique_ptr<RenderGraphPassData> data;
    };

    using RGPassPtr = std::unique_ptr<RenderGraphPassBase>;
}