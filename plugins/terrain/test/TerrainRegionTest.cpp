//
// Created on 2026/09/22.
//

#include <terrain/TerrainSystem.h>
#include <terrain/TerrainAddress.h>
#include <terrain/TerrainRegion.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::terrain;

namespace {

    TerrainAssetData MakeGridData(int32_t halfExtent)
    {
        TerrainAssetData data;
        data.meta.tileSize     = 8;
        data.meta.resolution   = 1.f;
        data.meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
        data.meta.heightScale  = 1.f;
        data.meta.heightOffset = 0.f;
        data.meta.lodCount     = 1;

        for (int32_t y = -halfExtent; y <= halfExtent; ++y) {
            for (int32_t x = -halfExtent; x <= halfExtent; ++x) {
                TerrainTilePayload tile;
                tile.coord = TerrainTileCoord{x, y};

                TerrainLodPayload lod;
                lod.height.resize(static_cast<size_t>(data.meta.GetLodVertexCount(0)) * sizeof(float), 0);
                tile.lods.push_back(std::move(lod));

                data.tiles.push_back(std::move(tile));
            }
        }
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

    class CollectSink : public ITerrainRegionSink {
    public:
        void OnTerrainTileLod0(const TerrainTileCoord &coord, const TerrainMeta &meta,
                               const float *heights, uint32_t vertexSize) override
        {
            (void)meta;
            (void)heights;
            tiles.push_back(coord);
            lastVertexSize = vertexSize;
        }

        std::vector<TerrainTileCoord> tiles;
        uint32_t                      lastVertexSize = 0;
    };

    class CountListener : public ITerrainChangeListener {
    public:
        void OnTerrainTilesChanged(const std::vector<TerrainTileCoord> &coords) override
        {
            last = coords;
            ++count;
        }

        std::vector<TerrainTileCoord> last;
        uint32_t                      count = 0;
    };

} // namespace

TEST(TerrainRegionTest, TileRangeForBounds)
{
    TerrainMeta meta;
    meta.tileSize   = 8;
    meta.resolution = 1.f;

    TerrainTileCoord minCoord;
    TerrainTileCoord maxCoord;

    TileRangeForBounds(meta, AABB(Vector3(2.f, 0.f, 2.f), Vector3(6.f, 0.f, 6.f)), minCoord, maxCoord);
    EXPECT_EQ(minCoord, (TerrainTileCoord{0, 0}));
    EXPECT_EQ(maxCoord, (TerrainTileCoord{0, 0}));

    TileRangeForBounds(meta, AABB(Vector3(2.f, 0.f, 2.f), Vector3(9.f, 0.f, 9.f)), minCoord, maxCoord);
    EXPECT_EQ(minCoord, (TerrainTileCoord{0, 0}));
    EXPECT_EQ(maxCoord, (TerrainTileCoord{1, 1}));
}

TEST(TerrainRegionTest, SampleRegionLod0)
{
    const auto data = MakeGridData(3);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);
    Settle(system);

    CollectSink sink;
    EXPECT_TRUE(system.SampleRegionLod0(AABB(Vector3(2.f, 0.f, 2.f), Vector3(6.f, 0.f, 6.f)), sink));
    ASSERT_EQ(sink.tiles.size(), 1u);
    EXPECT_EQ(sink.tiles[0], (TerrainTileCoord{0, 0}));
    EXPECT_EQ(sink.lastVertexSize, data.meta.GetTileVertexSize());

    // Region extending beyond loaded tiles reports incomplete coverage.
    CollectSink farSink;
    EXPECT_FALSE(system.SampleRegionLod0(AABB(Vector3(4.f, 0.f, 4.f), Vector3(200.f, 0.f, 200.f)), farSink));
}

TEST(TerrainRegionTest, NotifyTilesChanged)
{
    const auto data = MakeGridData(3);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);
    Settle(system);
    ASSERT_TRUE(system.IsTileLoaded({0, 0}, 0));

    CountListener listener;
    system.AddChangeListener(&listener);

    system.NotifyTilesChanged({{0, 0}});

    EXPECT_EQ(listener.count, 1u);
    ASSERT_EQ(listener.last.size(), 1u);
    EXPECT_EQ(listener.last[0], (TerrainTileCoord{0, 0}));
    EXPECT_FALSE(system.IsTileLoaded({0, 0}, 0));

    system.RemoveChangeListener(&listener);
}
