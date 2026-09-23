//
// Created on 2026/09/22.
//

#include <vegetation/VegetationSystem.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::vegetation;

namespace {

    class UniformSurface : public IVegetationSurfaceProvider {
    public:
        float cellSize = 8.f;

        bool SampleSurface(const Vector3 &, VegetationSurfaceSample &out) const override
        {
            out.height          = 0.f;
            out.normal          = VEC3_Y;
            out.layerWeights[0] = 1.f;
            out.valid           = true;
            return true;
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

    VegetationPalette MakePalette()
    {
        VegetationBiome biome;
        biome.id          = 1;
        biome.maxSlopeDeg = 90.f;
        biome.layerIndex  = -1;
        biome.density     = 1.f;
        biome.species.push_back(VegetationSpecies{});
        return VegetationPalette{{biome}};
    }

    void Settle(VegetationSystem &system)
    {
        for (int i = 0; i < 64; ++i) {
            system.Tick(0.f);
            TaskExecutor::Get()->WaitForAll();
            system.Tick(0.f);
            if (system.GetPendingCellCount() == 0 && system.GetLoadedCellCount() > 0) {
                break;
            }
        }
    }

} // namespace

TEST(VegetationStreamingTest, PagesCellsWithinRadius)
{
    UniformSurface surface;

    VegetationSystem system;
    system.SetSurfaceProvider(&surface);
    system.SetPalette(MakePalette());
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(15.f, 23.f);
    system.SetLoadBudget(100);

    Settle(system);

    EXPECT_EQ(system.GetLoadedCellCount(), 9u);
    EXPECT_TRUE(system.IsCellLoaded(0, 0));
}

TEST(VegetationStreamingTest, DistanceDensityLodThinsFarCells)
{
    UniformSurface surface;

    VegetationSystem system;
    system.SetSurfaceProvider(&surface);
    system.SetPalette(MakePalette());
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(15.f, 23.f);
    system.SetLoadBudget(100);
    Settle(system);

    const auto &cells = system.GetLoadedCells();
    ASSERT_TRUE(cells.count(0) != 0);
    ASSERT_TRUE(cells.count((1ull << 32) | 1u) != 0); // cell (1,1)

    const size_t nearCount = cells.at(0).instances.size();
    const size_t farCount  = cells.at((1ull << 32) | 1u).instances.size();

    EXPECT_EQ(nearCount, 64u);
    EXPECT_LT(farCount, nearCount);
    EXPECT_GT(farCount, 0u);
}

TEST(VegetationStreamingTest, UnloadsOnFocusMove)
{
    UniformSurface surface;

    VegetationSystem system;
    system.SetSurfaceProvider(&surface);
    system.SetPalette(MakePalette());
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(15.f, 23.f);
    system.SetLoadBudget(100);
    Settle(system);
    ASSERT_GT(system.GetLoadedCellCount(), 0u);

    system.SetStreamingFocus(Vector3(1000.f, 0.f, 1000.f));
    Settle(system);
    // Old cells are unloaded; new cells around the new focus are paged in.
    EXPECT_FALSE(system.IsCellLoaded(0, 0));
    EXPECT_GT(system.GetLoadedCellCount(), 0u);
}

TEST(VegetationStreamingTest, PerWorldIsolation)
{
    UniformSurface surface;

    VegetationSystem a;
    VegetationSystem b;
    a.SetSurfaceProvider(&surface);
    a.SetPalette(MakePalette());
    b.SetSurfaceProvider(&surface);
    b.SetPalette(MakePalette());

    a.SetStreamingEnabled(true);
    a.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    a.SetStreamingRadii(15.f, 23.f);
    a.SetLoadBudget(100);
    Settle(a);

    EXPECT_GT(a.GetLoadedCellCount(), 0u);
    EXPECT_EQ(b.GetLoadedCellCount(), 0u);
}

TEST(VegetationStreamingTest, SurfaceChangeInvalidatesCells)
{
    UniformSurface surface;

    VegetationSystem system;
    system.SetSurfaceProvider(&surface);
    system.SetPalette(MakePalette());
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(15.f, 23.f);
    system.SetLoadBudget(100);
    Settle(system);
    ASSERT_TRUE(system.IsCellLoaded(0, 0));

    system.NotifyRegionChanged(AABB(Vector3(0.f, -1000.f, 0.f), Vector3(8.f, 1000.f, 8.f)));
    EXPECT_FALSE(system.IsCellLoaded(0, 0));
}
