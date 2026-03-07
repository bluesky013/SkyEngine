//
// Created by SkyEngine on 2025/3/7.
//

#pragma once

#include <render/renderpass/RasterPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>

namespace sky {

    static constexpr std::string_view HEIGHT_FOG_OUTPUT = "HeightFogOutput";

    class HeightFogPass : public FullScreenPass {
    public:
        explicit HeightFogPass(const RDGfxTechPtr &tech);
        ~HeightFogPass() override = default;

        void Setup(rdg::RenderGraph &rdg, RenderScene &scene) override;
    };

} // namespace sky
