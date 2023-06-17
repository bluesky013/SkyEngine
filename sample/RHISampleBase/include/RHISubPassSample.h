#pragma once

#include <RHISampleBase.h>

namespace sky::rhi {

    class RHISubPassSample : public RHISampleBase {
    public:
        RHISubPassSample() = default;
        ~RHISubPassSample() = default;

        void SetupBase() override;
        void OnTick(float delta) override;
        void OnStop() override;
    private:
        rhi::RenderPassPtr tiedPass;
        rhi::FrameBufferPtr fb;
        std::vector<rhi::ImageViewPtr> subpassViews;
        rhi::ImageViewPtr depthView;
        rhi::ImageViewPtr stencilView;
        std::vector<ClearValue> fbClears;

        rhi::GraphicsPipelinePtr pso1;
        rhi::GraphicsPipelinePtr pso2;
        rhi::GraphicsPipelinePtr pso3;
        rhi::GraphicsPipelinePtr fullScreen;
        rhi::PipelineLayoutPtr fullScreenLayout;
        rhi::PipelineLayoutPtr subpassLayout1;
        rhi::PipelineLayoutPtr subpassLayout2;
        rhi::DescriptorSetPtr fullScreenSet;
        rhi::DescriptorSetPtr subpassSet1;
        rhi::DescriptorSetPtr subpassSet2;
    };

} // namespace sky::rhi