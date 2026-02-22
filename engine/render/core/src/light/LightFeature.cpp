//
// Created by blues on 2024/12/8.
//

#include <render/light/LightFeature.h>
#include <render/light/LightFeatureProcessor.h>
#include <render/Renderer.h>

namespace sky {

    void LightFeature::Init() // NOLINT
    {
        Renderer::Get()->RegisterRenderFeature<LightFeatureProcessor>();
    }

} // namespace sky