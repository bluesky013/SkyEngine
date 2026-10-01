//
// Aurora light system: collects the scene's main directional light.
//

#pragma once

#include <aurora/light/LightTypes.h>

#include <core/math/Matrix4.h>
#include <core/math/Vector3.h>

#include <vector>

namespace sky::aurora {

    class RenderScene;

    // World-space position from the world matrix translation column (m[3].xyz).
    Vector3 ExtractLightPosition(const Matrix4 &world);

    // World-space direction: the world matrix rotation applied to the local
    // forward axis (-Z), normalized. Falls back to -Z for a degenerate matrix.
    Vector3 ExtractLightDirection(const Matrix4 &world);

    class LocalLightSystem {
    public:
        LocalLightSystem()  = default;
        ~LocalLightSystem() = default;

        // Gathers every Light (directional / point / spot) with its WorldInfo
        // into GPU-facing LightRenderData, appending to `out`. `out` is owned
        // by the caller (renderer), not stored on the scene.
        static void Process(RenderScene &scene, std::vector<LightRenderData> &out);
    };

} // namespace sky::aurora
