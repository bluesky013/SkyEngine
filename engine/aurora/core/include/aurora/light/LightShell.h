//
// Aurora light shell: CPU-side core processing for light influence volumes and
// conservative rasterization. Pure math + geometry -- no RHI / pipeline
// dependency. Produces the bounding volumes and convex-hull "shell" geometry
// later consumed by a CPU cull or a conservative-rasterization light-volume
// pass.
//

#pragma once

#include <aurora/light/LightTypes.h>

#include <core/math/GeometryGenerator.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

#include <cstdint>
#include <vector>

namespace sky::aurora {

    struct WorldInfo;
    class SceneView;

    enum class LightVolumeType : uint8_t {
        NONE = 0,
        SPHERE, // point
        CONE,   // spot
        BOX,    // directional
    };

    // World-space influence volume of a light.
    struct LightVolume {
        LightVolumeType type = LightVolumeType::NONE;

        // SPHERE: center / radius.
        Vector3 center;
        float   radius = 0.f;

        // CONE: apex at the light position, axis toward the light direction,
        // base at apex + axis * height with baseRadius.
        Vector3 apex;
        Vector3 axis       = {0.f, -1.f, 0.f};
        float   height     = 0.f;
        float   baseRadius = 0.f;

        // BOX (directional): world-space box.
        AABB box;

        // Conservative world bounds that contain the whole volume.
        AABB bounds;
    };

    // Screen-space rectangle in pixels; `valid == false` means "covers the
    // whole viewport" (conservative fallback).
    struct ScreenRect {
        float minX  = 0.f;
        float minY  = 0.f;
        float maxX  = 0.f;
        float maxY  = 0.f;
        bool  valid = false;
    };

    // Conservative tile coverage: `tiles` holds linear tile indices
    // (y * tilesX + x), a superset of the light's true screen coverage.
    struct LightCoverage {
        uint32_t              tilesX = 0;
        uint32_t              tilesY = 0;
        std::vector<uint32_t> tiles;
    };

    // World-space conservative AABB containing the light's influence volume.
    AABB ComputeLightBounds(const Light &light, const WorldInfo &world);

    // Classify the light into an influence volume. `view` is required to size a
    // directional light's box to the view frustum; when null a large default
    // box is used.
    LightVolume ComputeLightVolume(const Light &light, const WorldInfo &world, const SceneView *view);

    // Convex-hull shell geometry for conservative rasterization: a sphere, cone
    // or box surface. Returns empty streams for LightVolumeType::NONE.
    GeometryStreams GenerateLightShellGeometry(const LightVolume &volume, uint32_t sectors = 32, uint32_t rings = 16);

    // Conservative screen-space rect. Full-viewport (valid == false) when any
    // volume corner is behind the camera or the volume is BOX.
    ScreenRect ComputeConservativeScreenRect(const LightVolume &volume, const Matrix4 &viewProj, const Vector2 &extent);

    // Conservative tile coverage via tile-granular rasterization of the shell
    // triangles (triangle screen AABB vs tile).
    LightCoverage ComputeConservativeCoverage(const LightVolume &volume, const Matrix4 &viewProj, const Vector2 &extent, uint32_t tileSize = 16);

} // namespace sky::aurora
