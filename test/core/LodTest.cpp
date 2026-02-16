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
        float lodBias = 1.0f;
    };

    inline float CalculateScreenSize(const AABB &worldBound, const Vector3 &viewPos, float fov, float /*screenHeight*/)
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

} // namespace lod_test
} // namespace sky

using namespace sky;
using namespace sky::lod_test;

TEST(LodTest, ScreenSizeCalculation)
{
    AABB bound(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    float fov = ToRadian(90.0f);

    // Camera at the object center → screen size should be 1.0
    {
        Vector3 viewPos(0, 0, 0);
        float size = CalculateScreenSize(bound, viewPos, fov, 1080.0f);
        ASSERT_FLOAT_EQ(size, 1.0f);
    }

    // Camera far away → screen size should be small
    {
        Vector3 viewPos(0, 0, 100);
        float size = CalculateScreenSize(bound, viewPos, fov, 1080.0f);
        ASSERT_GT(size, 0.0f);
        ASSERT_LT(size, 0.1f);
    }

    // Camera nearby → screen size should be larger
    {
        Vector3 viewPos(0, 0, 5);
        float sizeNear = CalculateScreenSize(bound, viewPos, fov, 1080.0f);
        Vector3 viewPosFar(0, 0, 50);
        float sizeFar = CalculateScreenSize(bound, viewPosFar, fov, 1080.0f);
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
    float fov = ToRadian(60.0f);

    float prevSize = 2.0f;
    for (float dist = 10.0f; dist <= 100.0f; dist += 10.0f) {
        Vector3 viewPos(0, 0, dist);
        float size = CalculateScreenSize(bound, viewPos, fov, 1080.0f);
        ASSERT_LT(size, prevSize);
        prevSize = size;
    }
}
