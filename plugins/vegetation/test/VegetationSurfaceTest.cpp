//
// Created on 2026/09/22.
//

#include <vegetation/VegetationSystem.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::vegetation;

namespace {

    // Synthetic flat surface: height 0, up normal, all weight on layer 0, 8m cells.
    class SyntheticSurface : public IVegetationSurfaceProvider {
    public:
        bool SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const override
        {
            out.height        = 0.f;
            out.normal        = VEC3_Y;
            out.layerWeights[0] = 1.f;
            out.valid         = true;
            return true;
        }

        float GetCellSize() const override { return 8.f; }

        void GetCellRange(const AABB &bounds, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY) const override
        {
            const float cell = GetCellSize();
            minX = static_cast<int32_t>(std::floor(bounds.min.x / cell));
            minY = static_cast<int32_t>(std::floor(bounds.min.z / cell));
            maxX = static_cast<int32_t>(std::floor(bounds.max.x / cell));
            maxY = static_cast<int32_t>(std::floor(bounds.max.z / cell));
        }

        AABB GetCellBounds(int32_t cellX, int32_t cellY) const override
        {
            const float cell = GetCellSize();
            const float x0 = static_cast<float>(cellX) * cell;
            const float y0 = static_cast<float>(cellY) * cell;
            return AABB(Vector3(x0, -1000.f, y0), Vector3(x0 + cell, 1000.f, y0 + cell));
        }
    };

} // namespace

TEST(VegetationSurfaceTest, SyntheticProviderSampling)
{
    SyntheticSurface surface;

    VegetationSurfaceSample sample;
    EXPECT_TRUE(surface.SampleSurface(Vector3(1.f, 0.f, 2.f), sample));
    EXPECT_TRUE(sample.valid);
    EXPECT_FLOAT_EQ(sample.height, 0.f);
    EXPECT_FLOAT_EQ(sample.normal.y, 1.f);
    EXPECT_FLOAT_EQ(sample.layerWeights[0], 1.f);
}

TEST(VegetationSurfaceTest, SystemWithoutProviderReportsUnavailable)
{
    VegetationSystem system;
    EXPECT_FALSE(system.HasSurface());

    VegetationSurfaceSample sample;
    EXPECT_FALSE(system.SampleSurface(Vector3(1.f, 0.f, 2.f), sample));
    EXPECT_FALSE(sample.valid);
}

TEST(VegetationSurfaceTest, SystemWithProviderSamples)
{
    SyntheticSurface surface;

    VegetationSystem system;
    system.SetSurfaceProvider(&surface);
    EXPECT_TRUE(system.HasSurface());

    VegetationSurfaceSample sample;
    EXPECT_TRUE(system.SampleSurface(Vector3(3.f, 0.f, 4.f), sample));
    EXPECT_TRUE(sample.valid);
}

TEST(VegetationSurfaceTest, CellRangeAndBounds)
{
    SyntheticSurface surface;

    int32_t minX = 0;
    int32_t minY = 0;
    int32_t maxX = 0;
    int32_t maxY = 0;
    surface.GetCellRange(AABB(Vector3(2.f, 0.f, 2.f), Vector3(9.f, 0.f, 9.f)), minX, minY, maxX, maxY);
    EXPECT_EQ(minX, 0);
    EXPECT_EQ(minY, 0);
    EXPECT_EQ(maxX, 1);
    EXPECT_EQ(maxY, 1);

    const AABB bounds = surface.GetCellBounds(1, 2);
    EXPECT_FLOAT_EQ(bounds.min.x, 8.f);
    EXPECT_FLOAT_EQ(bounds.min.z, 16.f);
    EXPECT_FLOAT_EQ(bounds.max.x, 16.f);
    EXPECT_FLOAT_EQ(bounds.max.z, 24.f);
}
