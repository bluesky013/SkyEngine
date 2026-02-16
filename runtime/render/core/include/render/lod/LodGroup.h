//
// Created by blues on 2025/2/16.
//

#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

namespace sky {

    static constexpr uint32_t MAX_LOD_LEVEL = 8;
    static constexpr uint32_t INVALID_LOD_LEVEL = ~(0U);

    struct LodLevel {
        float screenSize = 0.f;
    };

    struct LodConfig {
        std::vector<LodLevel> lodLevels;
        float lodBias = 1.0f;
    };

    inline float CalculateScreenSize(const AABB &worldBound, const Vector3 &viewPos, float fov, float screenHeight)
    {
        auto center = (worldBound.min + worldBound.max) * 0.5f;
        auto extent = (worldBound.max - worldBound.min) * 0.5f;
        float radius = extent.Length();

        auto diff = center - viewPos;
        float dist = diff.Length();

        if (dist <= radius) {
            return 1.0f;
        }

        float screenRadius = radius / (dist * std::tan(fov * 0.5f));
        return screenRadius * 2.0f;
    }

    inline uint32_t SelectLodLevel(const LodConfig &config, float screenSize)
    {
        float biasedSize = screenSize * config.lodBias;
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.lodLevels.size()); ++i) {
            if (biasedSize >= config.lodLevels[i].screenSize) {
                return i;
            }
        }
        return INVALID_LOD_LEVEL;
    }

} // namespace sky
