//
// Created by blues on 2025/2/3.
//

#pragma once

#include <render/renderpass/RasterPass.h>

namespace sky {

    class DepthPass : public RasterPass {
    public:
        explicit DepthPass(rhi::PixelFormat ds, rhi::SampleCount samples_);
        ~DepthPass() override = default;

        void SetLayout(const RDResourceLayoutPtr &layout_);
    private:
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;
    };

} // namespace sky
