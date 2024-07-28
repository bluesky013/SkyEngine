//
// Created by Zach Lee on 2023/8/20.
//

#pragma once

#include <rhi/Device.h>
#include <render/RenderPipeline.h>
#include <render/resource/ResourceGroup.h>

namespace sky {
    class RenderWindow;

    class DefaultForward : public RenderPipeline {
    public:
        DefaultForward() = default;
        ~DefaultForward() override = default;

        void SetOutput(RenderWindow *wnd);
        bool OnSetup(rdg::RenderGraph &rdg, const std::vector<RenderScene*> &scenes) override;

    private:
        uint32_t viewMask = 0;
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;
        RDResourceLayoutPtr forwardLayout;
        RDResourceLayoutPtr shadowLayout;

        RDUniformBufferPtr globalUbo;

        RDGfxTechPtr postTech;

        SceneView *shadowScene = nullptr;
    };

} // namespace sky
