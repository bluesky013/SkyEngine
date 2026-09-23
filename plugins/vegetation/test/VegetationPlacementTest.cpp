//
// Created on 2026/09/22.
//

#include <vegetation/VegetationPlacement.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::vegetation;

namespace {

    class TestSurface : public IVegetationSurfaceProvider {
    public:
        float   height = 0.f;
        Vector3 normal = VEC3_Y;
        float   layerWeights[4] = {1.f, 0.f, 0.f, 0.f};
        float   cellSize = 8.f;
        bool    valid = true;

        bool SampleSurface(const Vector3 &worldPos, VegetationSurfaceSample &out) const override
        {
            out.height = height;
            out.normal = normal;
            for (int i = 0; i < 4; ++i) {
                out.layerWeights[i] = layerWeights[i];
            }
            out.valid = valid;
            return valid;
        }

        float GetCellSize() const override { return cellSize; }

        void GetCellRange(const AABB &bounds, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY) const override
        {
            minX = static_cast<int32_t>(std::floor(bounds.min.x / cellSize));
            minY = static_cast<int32_t>(std::floor(bounds.min.z / cellSize));
            maxX = static_cast<int32_t>(std::floor(bounds.max.x / cellSize));
            maxY = static_cast<int32_t>(std::floor(bounds.max.z / cellSize));
        }

        AABB GetCellBounds(int32_t cellX, int32_t cellY) const override
        {
            const float x0 = static_cast<float>(cellX) * cellSize;
            const float y0 = static_cast<float>(cellY) * cellSize;
            return AABB(Vector3(x0, -1000.f, y0), Vector3(x0 + cellSize, 1000.f, y0 + cellSize));
        }
    };

    VegetationPalette MakePalette(float maxSlope, int32_t layerIndex = 0, float minLayerWeight = 0.5f, float density = 1.f)
    {
        VegetationBiome biome;
        biome.id             = 1;
        biome.maxSlopeDeg    = maxSlope;
        biome.layerIndex     = layerIndex;
        biome.minLayerWeight = minLayerWeight;
        biome.density        = density;
        biome.species.push_back(VegetationSpecies{});
        return VegetationPalette{{biome}};
    }

} // namespace

TEST(VegetationPlacementTest, Deterministic)
{
    TestSurface surface;
    const auto palette = MakePalette(45.f);
    const VegetationPlacementConfig config{7u, 1.f};

    std::vector<VegetationInstance> a;
    std::vector<VegetationInstance> b;
    const uint32_t countA = GenerateCellInstances(surface, config, palette, 0, 0, 1.f, a);
    const uint32_t countB = GenerateCellInstances(surface, config, palette, 0, 0, 1.f, b);

    EXPECT_EQ(countA, 64u);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_FLOAT_EQ(a[i].position.x, b[i].position.x);
        EXPECT_FLOAT_EQ(a[i].position.z, b[i].position.z);
        EXPECT_FLOAT_EQ(a[i].rotation, b[i].rotation);
        EXPECT_FLOAT_EQ(a[i].scale, b[i].scale);
        EXPECT_EQ(a[i].speciesIndex, b[i].speciesIndex);
    }
}

TEST(VegetationPlacementTest, SlopeGating)
{
    TestSurface surface;
    // 60 degree slope.
    surface.normal = Vector3(0.f, 0.5f, 0.f);

    const auto palette = MakePalette(30.f);

    std::vector<VegetationInstance> out;
    EXPECT_EQ(GenerateCellInstances(surface, VegetationPlacementConfig{}, palette, 0, 0, 1.f, out), 0u);
    EXPECT_FLOAT_EQ(QueryDensity(surface, palette, Vector3(1.f, 0.f, 1.f)), 0.f);
}

TEST(VegetationPlacementTest, LayerGating)
{
    TestSurface surface;
    surface.layerWeights[0] = 0.f;

    const auto palette = MakePalette(45.f, 0, 1.f);

    std::vector<VegetationInstance> out;
    EXPECT_EQ(GenerateCellInstances(surface, VegetationPlacementConfig{}, palette, 0, 0, 1.f, out), 0u);

    surface.layerWeights[0] = 1.f;
    std::vector<VegetationInstance> out2;
    EXPECT_EQ(GenerateCellInstances(surface, VegetationPlacementConfig{}, palette, 0, 0, 1.f, out2), 64u);
}

TEST(VegetationPlacementTest, DistanceDensityScalesCount)
{
    TestSurface surface;
    const auto palette = MakePalette(45.f);

    std::vector<VegetationInstance> full;
    std::vector<VegetationInstance> thinned;
    const uint32_t fullCount = GenerateCellInstances(surface, VegetationPlacementConfig{}, palette, 0, 0, 1.f, full);
    const uint32_t thinCount = GenerateCellInstances(surface, VegetationPlacementConfig{}, palette, 0, 0, 0.25f, thinned);

    EXPECT_EQ(fullCount, 64u);
    EXPECT_LT(thinCount, fullCount);
    EXPECT_GT(thinCount, 0u);
}

TEST(VegetationPlacementTest, QueryDensityReportsBiomeDensity)
{
    TestSurface surface;

    VegetationBiome biome;
    biome.id          = 3;
    biome.maxSlopeDeg = 45.f;
    biome.layerIndex  = 0;
    biome.density     = 0.75f;
    biome.species.push_back(VegetationSpecies{});

    VegetationPalette palette{{biome}};

    EXPECT_FLOAT_EQ(QueryDensity(surface, palette, Vector3(1.f, 0.f, 1.f)), 0.75f);

    surface.valid = false;
    EXPECT_FLOAT_EQ(QueryDensity(surface, palette, Vector3(1.f, 0.f, 1.f)), 0.f);
}
