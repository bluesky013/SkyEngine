//
// Created by blues on 2024/9/6.
//

#pragma once

#include <render/RenderScenePipeline.h>
#include <render/adaptor/pipeline/ForwardMSAAPass.h>
#include <render/adaptor/pipeline/PostProcessingPass.h>
#include <render/adaptor/pipeline/PresentPass.h>
#include <render/adaptor/pipeline/DepthPass.h>
#include <render/adaptor/pipeline/BRDFLutPass.h>
#include <render/adaptor/pipeline/ShadowMapPass.h>
#include <memory>

namespace sky {
    class RenderWindow;

    class DefaultForwardPipeline : public RenderScenePipeline {
    public:
        explicit DefaultForwardPipeline(RenderScene *scn) : RenderScenePipeline(scn) {}
        ~DefaultForwardPipeline() override = default;

        void SetOutput(RenderWindow *wnd);

    private:
        void InitPass();
        void SetupGlobal(rdg::RenderGraph &rdg, uint32_t w, uint32_t h);
        void SetupScreenExternalImages(rdg::RenderGraph &rdg, uint32_t w, uint32_t h);

        void Collect(rdg::RenderGraph &rdg) override;

        uint32_t viewMask = 0;
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D32;

        RDResourceLayoutPtr defaultRasterLayout;
        RDUniformBufferPtr defaultGlobal;

        rhi::ImagePtr hizDepth;
        rhi::SamplerPtr pointSampler;

        std::unique_ptr<DepthPass>          depth;
        std::unique_ptr<HizGenerator>       hiz;
        std::unique_ptr<ShadowMapPass>      shadowMap;
        std::unique_ptr<ForwardMSAAPass>    forward;
        std::unique_ptr<PostProcessingPass> postProcess;
        std::unique_ptr<PresentPass>        present;
        std::unique_ptr<BRDFLutPass>        brdfLut;
    };

} // namespace sky
