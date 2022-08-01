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

        static constexpr uint32_t FORWARD_TAG = 0x01;

        void ViewportChange(RenderViewport& vp) override;

        void BeginFrame() override;

        void DoFrame() override;

        void EndFrame() override;

    private:
        std::unique_ptr<FrameGraph> currentFrame;
        std::unique_ptr<FrameGraph> lastFrame;
        drv::ImagePtr msaaColor;
        drv::ImagePtr depthStencil;
        uint32_t currentImageIndex = 0;
    };

}