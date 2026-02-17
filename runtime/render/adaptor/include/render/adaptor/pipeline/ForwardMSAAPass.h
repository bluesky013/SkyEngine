//
// Created by blues on 2024/9/3.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class ForwardMSAAPass : public RasterPass {
    public:
        ForwardMSAAPass(rhi::PixelFormat color, rhi::PixelFormat ds, rhi::SampleCount samples_);
        ~ForwardMSAAPass() override = default;

        void SetLayout(const RDResourceLayoutPtr &layout);
    private:
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;

        rhi::PixelFormat colorFormat     = rhi::PixelFormat::RGBA8_UNORM;
        rhi::PixelFormat depthStenFormat = rhi::PixelFormat::D24_S8;
        rhi::SampleCount samples         = rhi::SampleCount::X2;
    };

} // namespace sky
