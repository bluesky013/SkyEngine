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
        std::vector<ClearValue> fbClears;

        rhi::GraphicsPipelinePtr pso1;
        rhi::GraphicsPipelinePtr pso2;
        rhi::GraphicsPipelinePtr fullScreen;
        rhi::PipelineLayoutPtr fullScreenLayout;
        rhi::DescriptorSetPtr fullScreenSet;
    };

} // namespace sky::rhi