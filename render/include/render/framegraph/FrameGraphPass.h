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
        FrameGraphPass() = default;
        ~FrameGraphPass() = default;

        virtual void UseImageAttachment(FrameGraphAttachment* attachment) = 0;
    };

    class FrameGraphGraphicPass : public FrameGraphPass {
    public:
        FrameGraphGraphicPass() = default;
        ~FrameGraphGraphicPass() = default;

        void UseImageAttachment(FrameGraphAttachment* attachment) override;
    };

}
