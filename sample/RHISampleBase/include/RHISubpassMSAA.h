//
// Created by Zach Lee on 2023/6/14.
//

#pragma once

#include <RHISampleBase.h>

namespace sky::rhi {

    class RHISubPassMSAA : public RHISampleBase {
    public:
        RHISubPassMSAA() = default;
        ~RHISubPassMSAA() = default;

        void SetupBase() override;
        void OnTick(float delta) override;
        void OnStop() override;
    private:
        rhi::RenderPassPtr tiedPass;
        rhi::FrameBufferPtr fb;

        rhi::ImageViewPtr ms1;
        rhi::ImageViewPtr resolve1;
        rhi::ImageViewPtr ms2;
        rhi::ImageViewPtr resolve2;
        rhi::ImageViewPtr ds;
        rhi::ImageViewPtr dsResolve;

        std::vector<ClearValue> fbClears;

        rhi::GraphicsPipelinePtr pso1;
        rhi::GraphicsPipelinePtr pso2;
        rhi::PipelineLayoutPtr fullScreenLayout;
        rhi::PipelineLayoutPtr emptyLayout;
        rhi::DescriptorSetPtr  set1;
    };

} // namespace sky::rhi
