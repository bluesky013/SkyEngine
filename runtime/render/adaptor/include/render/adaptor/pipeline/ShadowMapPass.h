//
// Created by blues on 2024/9/6.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    class ShadowMapPass : public RasterPass {
    public:
        explicit ShadowMapPass(uint32_t width, uint32_t height);
        ~ShadowMapPass() override = default;

        void SetLayout(const RDResourceLayoutPtr &layout_);
    private:
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;

        SceneView* sceneView = nullptr;
        Name shadowViewName;
    };

} // namespace sky
