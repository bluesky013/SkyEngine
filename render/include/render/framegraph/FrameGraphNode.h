//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <string>
#include <vulkan/CommandBuffer.h>

namespace sky {
    class FrameGraphNode {
    public:
        explicit FrameGraphNode(const std::string &str) : name(str)
        {
        }
        virtual ~FrameGraphNode() = default;

        virtual void Execute(const vk::CommandBufferPtr &commandBuffer) = 0;

    private:
        friend class FrameGraph;
        std::string name;
    };

} // namespace sky
