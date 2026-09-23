//
// Created on 2026/09/22.
//

#include <terrain/TerrainSystem.h>

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
        data.meta.layerCount   = 0;
        data.meta.lodCount     = 3;

        for (int32_t y = -halfExtent; y <= halfExtent; ++y) {
            for (int32_t x = -halfExtent; x <= halfExtent; ++x) {
                TerrainTilePayload tile;
                tile.coord = TerrainTileCoord{x, y};
                for (uint32_t lod = 0; lod < data.meta.lodCount; ++lod) {
                    TerrainLodPayload lodPayload;
                    lodPayload.height.resize(static_cast<size_t>(data.meta.GetLodVertexCount(lod)) * sizeof(float), 0);
                    tile.lods.push_back(std::move(lodPayload));
                }
                data.tiles.push_back(std::move(tile));

                TerrainTileInfo info{};
                info.coord    = tile.coord;
                info.lodCount = data.meta.lodCount;
                data.manifest.push_back(info);
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
            if (system.GetPendingLoadCount() == 0) {
                break;
            }
        }
    }

} // namespace

TEST(TerrainSystemTest, LoadsOneLodPerAnnulus)
{
    const auto data = MakeGridData(3);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);

    Settle(system);

    // LOD0 covers dist < 12 (9 tiles), LOD1 12..24 (16 tiles), LOD2 24..48 (24 tiles).
    EXPECT_EQ(system.GetLoadedLodCount(0), 9u);
    EXPECT_EQ(system.GetLoadedLodCount(1), 16u);
    EXPECT_EQ(system.GetLoadedLodCount(2), 24u);
    EXPECT_EQ(system.GetLoadedTileCount(), 49u);
    EXPECT_EQ(system.GetPendingLoadCount(), 0u);
}

TEST(TerrainSystemTest, FieldHoldsOnlyNearHighDetail)
{
    const auto data = MakeGridData(3);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);
    Settle(system);

    // CPU queries/collision field only holds LOD0 (near) tiles.
    EXPECT_EQ(system.GetField().GetLoadedTileCount(), system.GetLoadedLodCount(0));

    float height = 0.f;
    EXPECT_TRUE(system.GetField().QueryHeight(Vector3(4.f, 0.f, 4.f), height));
    // A far tile is loaded as a coarse LOD but has no CPU field data.
    EXPECT_FALSE(system.GetField().QueryHeight(Vector3(4.f, 0.f, 4.f + 40.f), height));
}

TEST(TerrainSystemTest, HysteresisKeepsFineLodInBand)
{
    const auto data = MakeGridData(0);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingRadii(6.f, 12.f);
    system.SetLoadBudget(10);

    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    Settle(system);
    ASSERT_TRUE(system.IsTileLoaded({0, 0}, 0));

    // Move focus so the tile sits inside the hysteresis gap (outer load 6, unload 12).
    system.SetStreamingFocus(Vector3(4.f, 0.f, 14.f));
    Settle(system);
    EXPECT_TRUE(system.IsTileLoaded({0, 0}, 0));   // fine LOD retained
    EXPECT_TRUE(system.IsTileLoaded({0, 0}, 1));   // coarser LOD added in its annulus

    // Move past the unload radius and the fine LOD is dropped.
    system.SetStreamingFocus(Vector3(4.f, 0.f, 18.f));
    Settle(system);
    EXPECT_FALSE(system.IsTileLoaded({0, 0}, 0));
}

TEST(TerrainSystemTest, UnloadsOnFocusMove)
{
    const auto data = MakeGridData(3);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);
    Settle(system);
    ASSERT_EQ(system.GetLoadedTileCount(), 49u);

    system.SetStreamingFocus(Vector3(1000.f, 0.f, 1000.f));
    Settle(system);
    EXPECT_EQ(system.GetLoadedTileCount(), 0u);
}

TEST(TerrainSystemTest, PerWorldIsolation)
{
    const auto data = MakeGridData(3);

    TerrainSystem a;
    TerrainSystem b;
    ASSERT_TRUE(a.Setup(data));
    ASSERT_TRUE(b.Setup(data));

    a.SetStreamingEnabled(true);
    a.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    a.SetStreamingRadii(12.f, 20.f);
    a.SetLoadBudget(200);
    Settle(a);

    EXPECT_GT(a.GetLoadedTileCount(), 0u);
    EXPECT_EQ(b.GetLoadedTileCount(), 0u);
}

TEST(TerrainSystemTest, InvalidateOnTerrainChanged)
{
    const auto data = MakeGridData(3);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);
    Settle(system);
    ASSERT_GT(system.GetLoadedTileCount(), 0u);

    system.OnTerrainChanged();

    EXPECT_EQ(system.GetLoadedTileCount(), 0u);
    EXPECT_EQ(system.GetPendingLoadCount(), 0u);

    float height = 0.f;
    EXPECT_FALSE(system.GetField().QueryHeight(Vector3(4.f, 0.f, 4.f), height));
}

TEST(TerrainSystemTest, HoleTilesAreNotLoaded)
{
    auto data = MakeGridData(1);

    TerrainTileInfo hole;
    hole.coord   = TerrainTileCoord{0, 0};
    hole.hasData = false;
    data.manifest.push_back(hole);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(100);
    Settle(system);

    EXPECT_FALSE(system.IsTileLoaded({0, 0}, 0));

    float height = 0.f;
    EXPECT_FALSE(system.GetField().QueryHeight(Vector3(4.f, 0.f, 4.f), height));
}

TEST(TerrainSystemTest, ResidencyDelta)
{
    const auto data = MakeGridData(2);

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(200);
    Settle(system);

    std::vector<TerrainTileLodRef> added;
    std::vector<TerrainTileLodRef> removed;
    EXPECT_TRUE(system.ConsumeResidencyDelta(added, removed));
    EXPECT_GT(added.size(), 0u);
    EXPECT_TRUE(removed.empty());

    added.clear();
    removed.clear();
    EXPECT_FALSE(system.ConsumeResidencyDelta(added, removed));   // no change since last consume

    system.SetStreamingFocus(Vector3(1000.f, 0.f, 1000.f));
    Settle(system);

    std::vector<TerrainTileLodRef> added2;
    std::vector<TerrainTileLodRef> removed2;
    EXPECT_TRUE(system.ConsumeResidencyDelta(added2, removed2));
    EXPECT_GT(removed2.size(), 0u);
}
