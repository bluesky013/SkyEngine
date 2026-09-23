//
// Created on 2026/09/22.
//

#include <vegetation/terrain/TerrainSurfaceProvider.h>

#include <terrain/TerrainSystem.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;
using namespace sky::vegetation;

namespace {

    TerrainAssetData MakeSingleTileData()
    {
        TerrainAssetData data;
        data.meta.tileSize     = 8;
        data.meta.resolution   = 1.f;
        data.meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
        data.meta.heightScale  = 1.f;
        data.meta.heightOffset = 0.f;
        data.meta.layerCount   = 1;
        data.meta.lodCount     = 1;

        TerrainTilePayload tile;
        tile.coord = TerrainTileCoord{0, 0};

        TerrainLodPayload lod;
        lod.height.resize(static_cast<size_t>(data.meta.GetLodVertexCount(0)) * sizeof(float), 0);
        const uint32_t size = data.meta.GetTileVertexSize();
        lod.splat.emplace_back(static_cast<size_t>(size) * size * 4u, 255);
        tile.lods.push_back(std::move(lod));

        data.tiles.push_back(std::move(tile));
        return data;
    }

    void Settle(TerrainSystem &system)
    {
        for (int i = 0; i < 64; ++i) {
            system.Tick(0.f);
            TaskExecutor::Get()->WaitForAll();
            system.Tick(0.f);
            if (system.GetPendingLoadCount() == 0 && system.GetLoadedTileCount() > 0) {
                break;
            }
        }
    }

    class CountListener : public IVegetationSurfaceListener {
    public:
        void OnVegetationSurfaceChanged(const AABB &region) override
        {
            last = region;
            ++count;
        }

        AABB     last;
        uint32_t count = 0;
    };

} // namespace

TEST(TerrainSurfaceProviderTest, SamplesTerrainAndAlignsCells)
{
    const auto data = MakeSingleTileData();

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(10);
    Settle(system);
    ASSERT_GT(system.GetLoadedTileCount(), 0u);

    TerrainSurfaceProvider provider(&system);

    VegetationSurfaceSample sample;
    ASSERT_TRUE(provider.SampleSurface(Vector3(4.f, 0.f, 4.f), sample));
    EXPECT_TRUE(sample.valid);
    EXPECT_FLOAT_EQ(sample.height, 0.f);
    EXPECT_NEAR(sample.normal.y, 1.f, 1e-3f);
    EXPECT_FLOAT_EQ(sample.layerWeights[0], 1.f);

    EXPECT_FLOAT_EQ(provider.GetCellSize(), 8.f);

    int32_t minX = 0;
    int32_t minY = 0;
    int32_t maxX = 0;
    int32_t maxY = 0;
    provider.GetCellRange(AABB(Vector3(2.f, 0.f, 2.f), Vector3(9.f, 0.f, 9.f)), minX, minY, maxX, maxY);
    EXPECT_EQ(minX, 0);
    EXPECT_EQ(minY, 0);
    EXPECT_EQ(maxX, 1);
    EXPECT_EQ(maxY, 1);

    const AABB cell = provider.GetCellBounds(1, 2);
    EXPECT_FLOAT_EQ(cell.min.x, 8.f);
    EXPECT_FLOAT_EQ(cell.min.z, 16.f);
    EXPECT_FLOAT_EQ(cell.max.x, 16.f);
    EXPECT_FLOAT_EQ(cell.max.z, 24.f);
}

TEST(TerrainSurfaceProviderTest, ForwardsTerrainChanges)
{
    const auto data = MakeSingleTileData();

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(10);
    Settle(system);

    TerrainSurfaceProvider provider(&system);
    CountListener listener;
    provider.AddSurfaceListener(&listener);

    system.NotifyTilesChanged({{0, 0}});

    EXPECT_EQ(listener.count, 1u);
    EXPECT_FLOAT_EQ(listener.last.min.x, 0.f);
    EXPECT_FLOAT_EQ(listener.last.max.z, 8.f);

    provider.RemoveSurfaceListener(&listener);
}
