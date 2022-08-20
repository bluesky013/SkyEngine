//
// Created by Zach Lee on 2022/7/18.
//

#pragma once

#include <render/RenderPipeline.h>
#include <render/framegraph/FrameGraph.h>

namespace sky {

    class RenderPipelineForward : public RenderPipeline {
    public:
        RenderPipelineForward(RenderScene& scene) : RenderPipeline(scene) {}
        ~RenderPipelineForward() = default;

        void ViewportChange(const RenderViewport& viewport) override;

        void SetOutput(const drv::ImagePtr& output) override;

        void BeginFrame(FrameGraph& frameGraph) override;

        void DoFrame(FrameGraph& frameGraph, const drv::CommandBufferPtr& cmdBuffer) override;

    private:
        drv::ImagePtr msaaColor;
        drv::ImagePtr colorOut;
        drv::ImagePtr depthStencil;
    };

}