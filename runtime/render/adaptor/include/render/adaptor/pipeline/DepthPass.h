//
// Created by blues on 2025/2/3.
//

#pragma once

#include <render/renderpass/RasterPass.h>

namespace sky {

    class DepthPass : public RasterPass {
    public:
        explicit DepthPass(uint32_t width, uint32_t height);
        ~DepthPass() override = default;

    private:
        void SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene) override;
    };

} // namespace sky
