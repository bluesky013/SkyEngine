//
// Created on 2026/10/20.
//

#pragma once

#include <array>
#include <aurora/renderer/IRenderFeature.h>

namespace sky::aurora {
    struct SceneViewSettings {
        uint32_t hdr                  = 0;
        float    exposure             = 0;
        float    environmentIntensity = 1;
    };
    // Diffuse irradiance, roughness-prefiltered specular, then BRDF integration; all are linear 2D images.
    using SceneEnvironmentInputs = std::array<std::optional<RenderResourceKey>, 3>;
    FrameExtensionPayload
    SceneViewInput(Name feature, ViewportId output, const SceneViewSettings &settings, const SceneEnvironmentInputs &environment = {});
    std::shared_ptr<IFrameGraphFeature> CreateSceneRenderFeature(Name name);
} // namespace sky::aurora
