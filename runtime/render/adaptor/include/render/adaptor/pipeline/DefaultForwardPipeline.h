//
// Created by blues on 2024/9/6.
//

#pragma once

#include <render/RenderScenePipeline.h>
#include <render/adaptor/pipeline/ForwardMSAAPass.h>
#include <render/adaptor/pipeline/PostProcessingPass.h>
#include <render/adaptor/pipeline/PresentPass.h>
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
        void Collect(rdg::RenderGraph &rdg) override;

        uint32_t viewMask = 0;
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;

        RDUniformBufferPtr globalUbo;

        std::unique_ptr<ForwardMSAAPass>    forward;
        std::unique_ptr<PostProcessingPass> postProcess;
        std::unique_ptr<PresentPass>        present;
    };

} // namespace sky
