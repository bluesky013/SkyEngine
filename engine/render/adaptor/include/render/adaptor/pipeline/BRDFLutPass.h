//
// Created by blues on 2025/2/17.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class BRDFLutPass : public FullScreenPass {
    public:
        explicit BRDFLutPass(const RDGfxTechPtr &tech);
        ~BRDFLutPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;
    private:
        RDResourceLayoutPtr debugLayout;

        Name brdfTextureName;
        rhi::ImagePtr brdfTexture;
        bool firstTime = true;
    };

} // namespace sky
