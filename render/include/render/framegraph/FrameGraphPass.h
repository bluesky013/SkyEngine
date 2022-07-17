//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <render/framegraph/FrameGraphNode.h>
#include <render/framegraph/FrameGraphAttachment.h>

namespace sky {
    class FrameGraphBuilder;

    class FrameGraphPass : public FrameGraphNode {
    public:
        FrameGraphPass(const std::string& str) : FrameGraphNode(str) {}
        ~FrameGraphPass() = default;

        virtual void Compile() {}

        virtual void UseImageAttachment(FrameGraphImageAttachment* attachment) = 0;
    };

    class FrameGraphGraphicPass : public FrameGraphPass {
    public:
        FrameGraphGraphicPass(const std::string& str) : FrameGraphPass(str) {}
        ~FrameGraphGraphicPass() = default;

        void UseImageAttachment(FrameGraphImageAttachment* attachment) override;

        void Compile() override;

        void Execute(drv::CommandBufferPtr commandBuffer) override;

    private:
        void AddClearValue(FrameGraphImageAttachment* attachment);

        drv::PassBeginInfo passInfo = {};
        std::vector<VkClearValue> clearValues;
        std::vector<FrameGraphImageAttachment*> colors;
        std::vector<FrameGraphImageAttachment*> resolves;
        std::vector<FrameGraphImageAttachment*> inputs;
        FrameGraphImageAttachment* depthStencil;
    };

}
