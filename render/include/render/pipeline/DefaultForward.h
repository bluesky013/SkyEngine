//
// Created by Zach Lee on 2023/8/20.
//

#pragma once

#include <render/RenderPipeline.h>
#include <rhi/Device.h>
#include <render/resource/ResourceGroup.h>

namespace sky {
    class RenderWindow;

    class DefaultForward : public RenderPipeline {
    public:
        DefaultForward() = default;
        ~DefaultForward() override = default;

        void SetOutput(RenderWindow *wnd);
        void OnSetup(rdg::RenderGraph &rdg) override;

    private:
        RenderWindow *output = nullptr;
        rhi::PixelFormat depthStencilFormat = rhi::PixelFormat::D24_S8;
        RDResourceLayoutPtr forwardLayout;
    };

} // namespace sky
