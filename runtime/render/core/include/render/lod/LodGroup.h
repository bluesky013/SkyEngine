//
// Created by blues on 2025/2/16.
//

#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>
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

    // UE-style: extract screen-size multiplier from projection matrix
    // ScreenMultiple = max(0.5 * ProjMatrix[0][0], 0.5 * ProjMatrix[1][1])
    inline float GetScreenMultiple(const Matrix4 &projMatrix)
    {
        return std::max(0.5f * projMatrix[0][0], 0.5f * projMatrix[1][1]);
    }

    // UE-style: ComputeBoundsScreenSize
    // ScreenSize = 2 * ScreenMultiple * SphereRadius / max(1, Distance)
    inline float ComputeBoundsScreenSize(float sphereRadius, float dist, float screenMultiple)
    {
        float screenRadius = screenMultiple * sphereRadius / std::max(1.0f, dist);
        return screenRadius * 2.0f;
    }

    inline float CalculateScreenSize(const AABB &worldBound, const Vector3 &viewPos, const Matrix4 &projMatrix)
    {
        auto center = (worldBound.min + worldBound.max) * 0.5f;
        auto extent = (worldBound.max - worldBound.min) * 0.5f;
        float radius = extent.Length();

        auto diff = center - viewPos;
        float dist = diff.Length();

        float screenMultiple = GetScreenMultiple(projMatrix);
        return ComputeBoundsScreenSize(radius, dist, screenMultiple);
    }

    // Convert a screen-size threshold to a distance threshold
    // From: ScreenSize = 2 * ScreenMultiple * SphereRadius / Distance
    // Solving: Distance = 2 * ScreenMultiple * SphereRadius / ScreenSize
    inline float ScreenSizeToDistance(float screenSize, float sphereRadius, float screenMultiple)
    {
        if (screenSize <= 0.f) {
            return std::numeric_limits<float>::max();
        }
        return (2.0f * screenMultiple * sphereRadius) / screenSize;
    }

    inline void PreComputeDistances(LodConfig &config, float sphereRadius, const Matrix4 &projMatrix)
    {
        float screenMultiple = GetScreenMultiple(projMatrix);
        config.distancesSq.resize(config.lodLevels.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.lodLevels.size()); ++i) {
            float d = ScreenSizeToDistance(config.lodLevels[i].screenSize, sphereRadius, screenMultiple);
            config.distancesSq[i] = d * d;
        }
    }

    inline float CalculateDistanceSq(const AABB &worldBound, const Vector3 &viewPos)
    {
        auto center = (worldBound.min + worldBound.max) * 0.5f;
        auto diff = center - viewPos;
        return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
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
