//
// Created by blues on 2024/9/5.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class PostProcessingPass : public FullScreenPass {
    public:
        explicit PostProcessingPass(const RDGfxTechPtr &tech);
        ~PostProcessingPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;

        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;

    private:
        RDResourceLayoutPtr debugLayout;
    };

} // namespace sky