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

    enum class LodPolicy : uint32_t {
        SCREEN_SIZE = 0,
        DISTANCE
    };

    struct LodLevel {
        float screenSize = 0.f;
        float distance = 0.f;
    };

    struct LodConfig {
        std::vector<LodLevel> lodLevels;
        float lodBias = 1.0f;
        LodPolicy policy = LodPolicy::SCREEN_SIZE;
    };

    inline float CalculateScreenSize(const AABB &worldBound, const Vector3 &viewPos, float fov)
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

    inline float CalculateDistance(const AABB &worldBound, const Vector3 &viewPos)
    {
        auto center = (worldBound.min + worldBound.max) * 0.5f;
        auto diff = center - viewPos;
        return diff.Length();
    }

    inline uint32_t SelectLodByScreenSize(const LodConfig &config, float screenSize)
    {
        float biasedSize = screenSize * config.lodBias;
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.lodLevels.size()); ++i) {
            if (biasedSize >= config.lodLevels[i].screenSize) {
                return i;
            }
        }
        return INVALID_LOD_LEVEL;
    }

    inline uint32_t SelectLodByDistance(const LodConfig &config, float distance)
    {
        float biasedDist = distance / std::max(config.lodBias, 0.001f);
        for (uint32_t i = static_cast<uint32_t>(config.lodLevels.size()); i > 0; --i) {
            if (biasedDist >= config.lodLevels[i - 1].distance) {
                return i - 1;
            }
        }
        return 0;
    }

    inline uint32_t SelectLodLevel(const LodConfig &config, float screenSize)
    {
        return SelectLodByScreenSize(config, screenSize);
    }

    inline uint32_t SelectLodLevel(const LodConfig &config, const AABB &worldBound, const Vector3 &viewPos, float fov)
    {
        if (config.policy == LodPolicy::DISTANCE) {
            float dist = CalculateDistance(worldBound, viewPos);
            return SelectLodByDistance(config, dist);
        }
        float size = CalculateScreenSize(worldBound, viewPos, fov);
        return SelectLodByScreenSize(config, size);
    }

} // namespace sky
