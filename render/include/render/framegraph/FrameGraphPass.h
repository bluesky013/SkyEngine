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

        virtual void UseImageAttachment(FrameGraphAttachment* attachment) = 0;
    };

    class FrameGraphGraphicPass : public FrameGraphPass {
    public:
        FrameGraphGraphicPass(const std::string& str) : FrameGraphPass(str) {}
        ~FrameGraphGraphicPass() = default;

        void UseImageAttachment(FrameGraphAttachment* attachment) override;
    };

}
