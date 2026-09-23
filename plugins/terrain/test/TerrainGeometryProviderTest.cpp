//
// Created on 2026/09/22.
//

#include <navigation/terrain/TerrainGeometryProvider.h>

#include <terrain/TerrainSystem.h>
#include <terrain/TerrainRegion.h>

#include <core/async/Task.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::ai;
using namespace sky::terrain;

namespace {

    class TestSink : public INaviGeometrySink {
    public:
        void AddTriangles(const Vector3 *inVertices, uint32_t vertexCount,
                          const uint32_t *inIndices, uint32_t indexCount) override
        {
            vertices.assign(inVertices, inVertices + vertexCount);
            indices.assign(inIndices, inIndices + indexCount);
            ++calls;
        }

        std::vector<Vector3>  vertices;
        std::vector<uint32_t> indices;
        uint32_t              calls = 0;
    };

    TerrainAssetData MakeSingleTileData()
    {
        TerrainAssetData data;
        data.meta.tileSize     = 8;
        data.meta.resolution   = 1.f;
        data.meta.heightFormat = TerrainHeightFormat::R32_SFLOAT;
        data.meta.heightScale  = 1.f;
        data.meta.heightOffset = 0.f;
        data.meta.lodCount     = 1;

        TerrainTilePayload tile;
        tile.coord = TerrainTileCoord{0, 0};
        TerrainLodPayload lod;
        lod.height.resize(static_cast<size_t>(data.meta.GetLodVertexCount(0)) * sizeof(float), 0);
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

} // namespace

TEST(TerrainGeometryProviderTest, EmitsUpwardTriangles)
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

    TerrainGeometryProvider provider(&system);
    TestSink sink;

    const AABB bounds(Vector3(0.f, -100.f, 0.f), Vector3(7.f, 100.f, 7.f));
    EXPECT_TRUE(provider.Collect(bounds, sink));

    // 8x8 quads -> 64 quads -> 256 vertices, 384 indices.
    EXPECT_EQ(sink.calls, 1u);
    EXPECT_EQ(sink.vertices.size(), 256u);
    EXPECT_EQ(sink.indices.size(), 384u);

    // First triangle winding faces up (+Y).
    const Vector3 &a = sink.vertices[sink.indices[0]];
    const Vector3 &b = sink.vertices[sink.indices[1]];
    const Vector3 &c = sink.vertices[sink.indices[2]];
    const Vector3  ab = b - a;
    const Vector3  ac = c - a;
    const float    normalY = ab.z * ac.x - ab.x * ac.z;
    EXPECT_GT(normalY, 0.f);
}

TEST(TerrainGeometryProviderTest, ReportsIncompleteCoverage)
{
    const auto data = MakeSingleTileData();

    TerrainSystem system;
    ASSERT_TRUE(system.Setup(data));
    system.SetStreamingEnabled(true);
    system.SetStreamingFocus(Vector3(4.f, 0.f, 4.f));
    system.SetStreamingRadii(12.f, 20.f);
    system.SetLoadBudget(10);
    Settle(system);

    TerrainGeometryProvider provider(&system);
    TestSink sink;

    // Extends far beyond the single loaded tile.
    EXPECT_FALSE(provider.Collect(AABB(Vector3(0.f, -100.f, 0.f), Vector3(64.f, 100.f, 64.f)), sink));
}
