//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <vulkan/CommandBuffer.h>
#include <string>

namespace sky {
    class FrameGraphNode {
    public:
        FrameGraphNode(const std::string& str) : name(str) {}
        virtual ~FrameGraphNode() = default;

        virtual void Execute(drv::CommandBufferPtr commandBuffer) = 0;

    private:
        friend class FrameGraph;
        std::string name;
    };

}