//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <render/RenderEncoder.h>
#include <render/RenderMeshPrimtive.h>
#include <render/framegraph/FrameGraphAttachment.h>
#include <render/framegraph/FrameGraphNode.h>

namespace sky {
    class FrameGraphBuilder;

    class FrameGraphPass : public FrameGraphNode {
    public:
        explicit FrameGraphPass(const std::string &str) : FrameGraphNode(str)
        {
        }
        ~FrameGraphPass() override = default;

        virtual void Compile() = 0;

        virtual void UseImageAttachment(FrameGraphImageAttachment *attachment) = 0;
    };

    class FrameGraphEmptyPass : public FrameGraphPass {
    public:
        explicit FrameGraphEmptyPass(const std::string &str) : FrameGraphPass(str)
        {
        }
        ~FrameGraphEmptyPass() override = default;

        void Compile() override
        {
        }

        void Execute(const vk::CommandBufferPtr &commandBuffer) override
        {
        }

        void UseImageAttachment(FrameGraphImageAttachment *attachment) override
        {
        }
    };

    class FrameGraphGraphicPass : public FrameGraphPass {
    public:
        explicit FrameGraphGraphicPass(const std::string &str) : FrameGraphPass(str)
        {
        }
        ~FrameGraphGraphicPass() override;

        void UseImageAttachment(FrameGraphImageAttachment *attachment) override;

        void Compile() override;

        void Execute(const vk::CommandBufferPtr &commandBuffer) override;

        void SetEncoder(RenderEncoder *encoder);

        vk::RenderPassPtr GetPass() const;

    private:
        void AddClearValue(FrameGraphImageAttachment *attachment);

        vk::PassBeginInfo                       passInfo = {};
        std::vector<VkClearValue>                clearValues;
        std::vector<FrameGraphImageAttachment *> colors;
        std::vector<FrameGraphImageAttachment *> resolves;
        std::vector<FrameGraphImageAttachment *> inputs;
        FrameGraphImageAttachment               *depthStencil = nullptr;
        RenderEncoder                           *encoder      = nullptr;
    };

} // namespace sky
