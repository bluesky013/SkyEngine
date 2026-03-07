//
// Created by Copilot on 2026/3/7.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class TSAAPass : public FullScreenPass {
    public:
        TSAAPass(const RDGfxTechPtr &tech, rhi::PixelFormat colorFormat);
        ~TSAAPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;

    private:
        Name tsaaColorName;
        Name tsaaHistoryName;

        rhi::ImagePtr historyImages[2];
        uint32_t writeIndex = 0;
        bool firstFrame = true;

        rhi::PixelFormat format;
    };

} // namespace sky
