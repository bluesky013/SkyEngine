//
// Created by blues on 2025/2/16.
//

#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
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
        std::vector<float> distancesSq;
        float lodBias = 1.0f;
    };

    inline float ScreenSizeToDistance(float screenSize, float radius, float halfTanFov)
    {
        if (screenSize <= 0.f) {
            return std::numeric_limits<float>::max();
        }
        return (2.0f * radius) / (screenSize * halfTanFov);
    }

    inline void PreComputeDistances(LodConfig &config, float radius, float fov)
    {
        float halfTanFov = std::tan(fov * 0.5f);
        config.distancesSq.resize(config.lodLevels.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.lodLevels.size()); ++i) {
            float d = ScreenSizeToDistance(config.lodLevels[i].screenSize, radius, halfTanFov);
            config.distancesSq[i] = d * d;
        }
    }

    inline float CalculateDistance(const AABB &worldBound, const Vector3 &viewPos)
    {
        auto center = (worldBound.min + worldBound.max) * 0.5f;
        auto diff = center - viewPos;
        return diff.Length();
    }

    inline float CalculateDistanceSq(const AABB &worldBound, const Vector3 &viewPos)
    {
        auto center = (worldBound.min + worldBound.max) * 0.5f;
        auto diff = center - viewPos;
        return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    }

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

    inline uint32_t SelectLodLevel(const LodConfig &config, float screenSize)
    {
        return SelectLodByScreenSize(config, screenSize);
    }

    inline uint32_t SelectLodLevel(const LodConfig &config, const AABB &worldBound, const Vector3 &viewPos)
    {
        if (config.distancesSq.empty()) {
            return INVALID_LOD_LEVEL;
        }

        float distSq = CalculateDistanceSq(worldBound, viewPos);
        float biasInv = 1.0f / std::max(config.lodBias, 0.001f);
        float biasedDistSq = distSq * biasInv * biasInv;

        for (uint32_t i = 0; i < static_cast<uint32_t>(config.distancesSq.size()); ++i) {
            if (biasedDistSq < config.distancesSq[i]) {
                return i;
            }
        }
        return INVALID_LOD_LEVEL;
    }

} // namespace sky
