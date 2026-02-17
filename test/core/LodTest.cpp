//
// Created by blues on 2025/2/16.
//

#include <gtest/gtest.h>
#include <core/math/MathUtil.h>
#include <core/shapes/AABB.h>

// Inline LOD functions for testing (duplicated from render/lod/LodGroup.h to avoid render dependency)
namespace sky {
namespace lod_test {

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
    inline float GetScreenMultiple(const Matrix4 &projMatrix)
    {
        return std::max(0.5f * projMatrix[0][0], 0.5f * projMatrix[1][1]);
    }

    // UE-style: ComputeBoundsScreenSize
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

} // namespace lod_test
} // namespace sky

using namespace sky;
using namespace sky::lod_test;

TEST(LodTest, ScreenSizeCalculation)
{
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);

    // Camera at the object center → screen size should be large (clamped by max(1,dist))
    {
        Vector3 viewPos(0, 0, 0);
        float size = CalculateScreenSize(bound, viewPos, projMatrix);
        ASSERT_GT(size, 1.0f);
    }

    // Camera far away → screen size should be small
    {
        Vector3 viewPos(0, 0, 100);
        float size = CalculateScreenSize(bound, viewPos, projMatrix);
        ASSERT_GT(size, 0.0f);
        ASSERT_LT(size, 0.1f);
    }

    // Camera nearby → screen size should be larger than far away
    {
        Vector3 viewPos(0, 0, 5);
        float sizeNear = CalculateScreenSize(bound, viewPos, projMatrix);
        Vector3 viewPosFar(0, 0, 50);
        float sizeFar = CalculateScreenSize(bound, viewPosFar, projMatrix);
        ASSERT_GT(sizeNear, sizeFar);
    }
}

TEST(LodTest, SelectLodLevelBasic)
{
    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {
        LodLevel{0.5f},   // LOD 0: screenSize >= 0.5
        LodLevel{0.25f},  // LOD 1: screenSize >= 0.25
        LodLevel{0.1f},   // LOD 2: screenSize >= 0.1
        LodLevel{0.0f},   // LOD 3: screenSize >= 0.0 (always matches as fallback)
    };

    // Large screen size → should select LOD 0
    ASSERT_EQ(SelectLodLevel(config, 0.8f), 0u);

    // Medium screen size → should select LOD 1
    ASSERT_EQ(SelectLodLevel(config, 0.3f), 1u);

    // Small screen size → should select LOD 2
    ASSERT_EQ(SelectLodLevel(config, 0.15f), 2u);

    // Very small screen size → should select LOD 3
    ASSERT_EQ(SelectLodLevel(config, 0.05f), 3u);

    // Exact boundary → should select that LOD
    ASSERT_EQ(SelectLodLevel(config, 0.5f), 0u);
    ASSERT_EQ(SelectLodLevel(config, 0.25f), 1u);
    ASSERT_EQ(SelectLodLevel(config, 0.1f), 2u);
}

TEST(LodTest, SelectLodLevelWithBias)
{
    LodConfig config;
    config.lodLevels = {
        LodLevel{0.5f},
        LodLevel{0.25f},
        LodLevel{0.1f},
    };

    // Normal bias (1.0)
    config.lodBias = 1.0f;
    ASSERT_EQ(SelectLodLevel(config, 0.3f), 1u);

    // Higher bias (2.0) makes effective screen size larger, selecting higher quality LOD
    config.lodBias = 2.0f;
    ASSERT_EQ(SelectLodLevel(config, 0.3f), 0u); // 0.3 * 2.0 = 0.6 >= 0.5

    // Lower bias (0.5) makes effective screen size smaller, selecting lower quality LOD
    config.lodBias = 0.5f;
    ASSERT_EQ(SelectLodLevel(config, 0.3f), 2u); // 0.3 * 0.5 = 0.15 >= 0.1
}

TEST(LodTest, SelectLodLevelEmpty)
{
    LodConfig config;
    config.lodBias = 1.0f;

    // Empty config → should return INVALID
    ASSERT_EQ(SelectLodLevel(config, 0.5f), INVALID_LOD_LEVEL);
}

TEST(LodTest, SelectLodLevelSingleLevel)
{
    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {LodLevel{0.0f}};

    // Single level with 0 threshold → always matches
    ASSERT_EQ(SelectLodLevel(config, 0.5f), 0u);
    ASSERT_EQ(SelectLodLevel(config, 0.01f), 0u);
    ASSERT_EQ(SelectLodLevel(config, 0.0f), 0u);
}

TEST(LodTest, ScreenSizeDecreasesWithDistance)
{
    AABB bound(Vector3(-2, -2, -2), Vector3(2, 2, 2));
    auto projMatrix = MakePerspective(ToRadian(60.0f), 1.0f, 0.1f, 1000.f);

    float prevSize = std::numeric_limits<float>::max();
    for (float dist = 10.0f; dist <= 100.0f; dist += 10.0f) {
        Vector3 viewPos(0, 0, dist);
        float size = CalculateScreenSize(bound, viewPos, projMatrix);
        ASSERT_LT(size, prevSize);
        prevSize = size;
    }
}

// ===== UE-style screen multiplier tests =====

TEST(LodTest, GetScreenMultipleFromProjMatrix)
{
    // 90° FOV, aspect 1.0: inverseHalfTan = 1/tan(45°) = 1.0
    auto proj90 = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);
    float sm90 = GetScreenMultiple(proj90);
    ASSERT_NEAR(sm90, 0.5f, 0.001f); // max(0.5*1.0, 0.5*1.0) = 0.5

    // 60° FOV, aspect 1.0: inverseHalfTan = 1/tan(30°) ≈ 1.732
    auto proj60 = MakePerspective(ToRadian(60.0f), 1.0f, 0.1f, 1000.f);
    float sm60 = GetScreenMultiple(proj60);
    ASSERT_GT(sm60, sm90); // Narrower FOV → larger screen multiple

    // With aspect != 1.0
    auto projWide = MakePerspective(ToRadian(90.0f), 16.0f/9.0f, 0.1f, 1000.f);
    float smWide = GetScreenMultiple(projWide);
    // [0][0] = inverseHalfTan / aspect < inverseHalfTan = [1][1]
    // max(0.5*[0][0], 0.5*[1][1]) = 0.5 * [1][1] = 0.5
    ASSERT_NEAR(smWide, 0.5f, 0.001f);
}

TEST(LodTest, ScreenSizeToDistanceConversion)
{
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);
    float screenMultiple = GetScreenMultiple(projMatrix);
    float radius = 1.7320508f; // sqrt(3) ~ extent of unit AABB

    // dist = 2 * screenMultiple * radius / screenSize
    {
        float d = ScreenSizeToDistance(0.5f, radius, screenMultiple);
        float expected = (2.0f * screenMultiple * radius) / 0.5f;
        ASSERT_FLOAT_EQ(d, expected);
    }

    // screenSize 0 → infinite distance
    {
        float d = ScreenSizeToDistance(0.0f, radius, screenMultiple);
        ASSERT_EQ(d, std::numeric_limits<float>::max());
    }

    // Smaller screenSize → larger distance
    {
        float d1 = ScreenSizeToDistance(0.5f, radius, screenMultiple);
        float d2 = ScreenSizeToDistance(0.25f, radius, screenMultiple);
        ASSERT_GT(d2, d1);
    }
}

TEST(LodTest, PreComputeDistancesMatchesScreenSize)
{
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto extent = (bound.max - bound.min) * 0.5f;
    float radius = extent.Length();
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);

    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {
        LodLevel{0.5f},
        LodLevel{0.25f},
        LodLevel{0.1f},
        LodLevel{0.0f},
    };

    PreComputeDistances(config, radius, projMatrix);

    // Test at various distances: both methods should agree
    for (float dist = 5.0f; dist <= 200.0f; dist += 5.0f) {
        Vector3 viewPos(0, 0, dist);
        float screenSize = CalculateScreenSize(bound, viewPos, projMatrix);

        uint32_t lodByScreen = SelectLodByScreenSize(config, screenSize);
        uint32_t lodByDist = SelectLodLevel(config, bound, viewPos);

        // For the fallback LOD (screenSize=0 → infinite distance), distance-based
        // returns INVALID while screen-size returns 3. Both are valid.
        if (lodByScreen == 3u && lodByDist == INVALID_LOD_LEVEL) {
            continue;
        }
        ASSERT_EQ(lodByScreen, lodByDist)
            << "Mismatch at distance " << dist
            << " (screenSize=" << screenSize << ")";
    }
}

TEST(LodTest, PreComputeDistancesWithBias)
{
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto extent = (bound.max - bound.min) * 0.5f;
    float radius = extent.Length();
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);

    LodConfig config;
    config.lodLevels = {
        LodLevel{0.5f},
        LodLevel{0.25f},
        LodLevel{0.1f},
    };
    PreComputeDistances(config, radius, projMatrix);

    Vector3 viewPos(0, 0, 10);

    // Normal bias
    config.lodBias = 1.0f;
    uint32_t lodNormal = SelectLodLevel(config, bound, viewPos);

    // Higher bias → should select higher quality (lower LOD index)
    config.lodBias = 2.0f;
    uint32_t lodHigh = SelectLodLevel(config, bound, viewPos);
    ASSERT_LE(lodHigh, lodNormal);

    // Lower bias → should select lower quality (higher LOD index)
    config.lodBias = 0.5f;
    uint32_t lodLow = SelectLodLevel(config, bound, viewPos);
    ASSERT_GE(lodLow, lodNormal);
}

TEST(LodTest, PreComputeDistancesEmpty)
{
    LodConfig config;
    config.lodBias = 1.0f;

    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    Vector3 viewPos(0, 0, 10);

    // No pre-computed distances → should return INVALID
    ASSERT_EQ(SelectLodLevel(config, bound, viewPos), INVALID_LOD_LEVEL);
}

TEST(LodTest, RuntimeSelectionOnlyUsesDistance)
{
    AABB bound(Vector3(-2, -2, -2), Vector3(2, 2, 2));
    auto extent = (bound.max - bound.min) * 0.5f;
    float radius = extent.Length();
    auto projMatrix = MakePerspective(ToRadian(60.0f), 1.0f, 0.1f, 1000.f);

    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {
        LodLevel{0.6f},
        LodLevel{0.3f},
        LodLevel{0.1f},
    };
    PreComputeDistances(config, radius, projMatrix);

    // Close camera → LOD 0
    {
        Vector3 viewPos(0, 0, 5);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 0u);
    }

    // Medium distance → LOD 1
    {
        Vector3 viewPos(0, 0, 20);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 1u);
    }

    // Far distance → LOD 2
    {
        Vector3 viewPos(0, 0, 80);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 2u);
    }
}

TEST(LodTest, DifferentAspectRatios)
{
    // Verify that screen size handles different aspect ratios correctly
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    Vector3 viewPos(0, 0, 20);

    auto proj1to1 = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);
    auto proj16to9 = MakePerspective(ToRadian(90.0f), 16.0f/9.0f, 0.1f, 1000.f);

    float size1to1 = CalculateScreenSize(bound, viewPos, proj1to1);
    float size16to9 = CalculateScreenSize(bound, viewPos, proj16to9);

    // Both should be positive
    ASSERT_GT(size1to1, 0.0f);
    ASSERT_GT(size16to9, 0.0f);

    // For wider aspect, [1][1] is the same but [0][0] is smaller
    // GetScreenMultiple uses max(), so same FOV → same screen multiple
    // when [1][1] >= [0][0]
    ASSERT_FLOAT_EQ(size1to1, size16to9);
}

// ===== LOD culling tests =====

TEST(LodTest, LodCullingWhenBeyondAllThresholds)
{
    // When no LOD level has a screenSize=0 fallback, objects beyond all
    // thresholds should return INVALID_LOD_LEVEL (mesh should be culled)
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto extent = (bound.max - bound.min) * 0.5f;
    float radius = extent.Length();
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);

    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {
        LodLevel{0.5f},   // LOD 0: close
        LodLevel{0.25f},  // LOD 1: medium
        LodLevel{0.1f},   // LOD 2: far (but not infinite)
    };
    PreComputeDistances(config, radius, projMatrix);

    // Close → valid LOD
    {
        Vector3 viewPos(0, 0, 5);
        uint32_t lod = SelectLodLevel(config, bound, viewPos);
        ASSERT_NE(lod, INVALID_LOD_LEVEL);
    }

    // Very far → should return INVALID_LOD_LEVEL (culled)
    {
        Vector3 viewPos(0, 0, 10000);
        uint32_t lod = SelectLodLevel(config, bound, viewPos);
        ASSERT_EQ(lod, INVALID_LOD_LEVEL);
    }
}

TEST(LodTest, LodCullingTransition)
{
    // Test the transition from visible to culled and back
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto extent = (bound.max - bound.min) * 0.5f;
    float radius = extent.Length();
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);

    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {
        LodLevel{0.5f},
        LodLevel{0.1f},
    };
    PreComputeDistances(config, radius, projMatrix);

    // Moving away: LOD 0 → LOD 1 → CULLED
    {
        Vector3 viewPos(0, 0, 2);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 0u);
    }
    {
        Vector3 viewPos(0, 0, 10);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 1u);
    }
    {
        Vector3 viewPos(0, 0, 10000);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), INVALID_LOD_LEVEL);
    }

    // Moving back: CULLED → LOD 1 → LOD 0 (streaming back in)
    {
        Vector3 viewPos(0, 0, 10);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 1u);
    }
    {
        Vector3 viewPos(0, 0, 2);
        ASSERT_EQ(SelectLodLevel(config, bound, viewPos), 0u);
    }
}

TEST(LodTest, LodCullingWithScreenSizeFallback)
{
    // When the last LOD has screenSize=0 (fallback), it should always
    // match — no culling occurs
    LodConfig config;
    config.lodBias = 1.0f;
    config.lodLevels = {
        LodLevel{0.5f},
        LodLevel{0.0f},  // fallback: always matches via screen-size
    };

    // Screen-size based: even tiny sizes match the fallback
    ASSERT_EQ(SelectLodLevel(config, 0.001f), 1u);
    ASSERT_EQ(SelectLodLevel(config, 0.0f), 1u);
}

TEST(LodTest, LodCullingWithBias)
{
    // Bias should affect the culling threshold
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto extent = (bound.max - bound.min) * 0.5f;
    float radius = extent.Length();
    auto projMatrix = MakePerspective(ToRadian(90.0f), 1.0f, 0.1f, 1000.f);

    LodConfig config;
    config.lodLevels = {
        LodLevel{0.5f},
        LodLevel{0.1f},
    };
    PreComputeDistances(config, radius, projMatrix);

    // Find a distance where it's culled with bias=1.0
    Vector3 farViewPos(0, 0, 10000);
    config.lodBias = 1.0f;
    ASSERT_EQ(SelectLodLevel(config, bound, farViewPos), INVALID_LOD_LEVEL);

    // With higher bias, the same distance may no longer be culled
    // (higher bias = keep higher quality longer = extend visible range)
    config.lodBias = 100.0f;
    uint32_t lodHighBias = SelectLodLevel(config, bound, farViewPos);
    ASSERT_NE(lodHighBias, INVALID_LOD_LEVEL);
}
