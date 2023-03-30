//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <vector>

namespace sky::rhi {

    class RenderPass {
    public:
        RenderPass()          = default;
        virtual ~RenderPass() = default;

        struct Attachment {
            PixelFormat format       = PixelFormat::UNDEFINED;
            SampleCount sample       = SampleCount::X1;
            LoadOp      load         = LoadOp::DONT_CARE;
            StoreOp     store        = StoreOp::DONT_CARE;
            LoadOp      stencilLoad  = LoadOp::DONT_CARE;
            StoreOp     stencilStore = StoreOp::DONT_CARE;
        };

        struct SubPass {
            std::vector<uint32_t> colors;
            std::vector<uint32_t> resolves;
            std::vector<uint32_t> inputs;
            uint32_t              depthStencil = ~(0U);
            uint32_t              viewMask     = 0;
        };

        struct Dependency {
            uint32_t src = (~0U);
            uint32_t dst = (~0U);
            PipelineStageFlags srcStage;
            PipelineStageFlags dstStage;
            AccessFlag srcFlag = AccessFlag::NONE;
            AccessFlag dstFlag = AccessFlag::NONE;
        };

        struct Descriptor {
            std::vector<Attachment> attachments;
            std::vector<SubPass>    subPasses;
            std::vector<Dependency> dependencies;
            std::vector<uint32_t>   correlatedViewMasks;
        };
    };

    using RenderPassPtr = std::shared_ptr<RenderPass>;

};
