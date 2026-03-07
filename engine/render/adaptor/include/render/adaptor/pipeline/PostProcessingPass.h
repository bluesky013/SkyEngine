//
// Created by blues on 2024/9/5.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class PostProcessingPass : public FullScreenPass {
    public:
        PostProcessingPass(const RDGfxTechPtr &tech, std::string_view inputColorName = FWD_CL);
        ~PostProcessingPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;

        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;

    private:
        RDResourceLayoutPtr debugLayout;
    };

} // namespace sky