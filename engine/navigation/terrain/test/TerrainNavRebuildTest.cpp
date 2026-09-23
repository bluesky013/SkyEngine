//
// Created on 2026/09/23.
//

#include <navigation/terrain/TerrainNavRebuild.h>

#include <navigation/NaviMeshFactory.h>

#include <terrain/TerrainQuery.h>
#include <terrain/TerrainSystemInterface.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using namespace sky;
using namespace sky::ai;
using namespace sky::terrain;

namespace {

    class FakeField : public ITerrainField {
    public:
        explicit FakeField(const TerrainMeta &inMeta) : meta(inMeta) {}

        const TerrainMeta &GetMeta() const override { return meta; }
        uint32_t GetLoadedTileCount() const override { return 0; }
        bool GetTileHeights(const TerrainTileCoord &, const float *&, uint32_t &) const override { return false; }
        bool QueryHeight(const Vector3 &, float &) const override { return false; }
        bool QueryNormal(const Vector3 &, Vector3 &) const override { return false; }
        bool QuerySplatWeights(const Vector3 &, Vector4 &) const override { return false; }
        bool Raycast(const Vector3 &, const Vector3 &, float, TerrainRaycastHit &) const override { return false; }

    private:
        TerrainMeta meta;
    };

    class FakeTerrainSystem : public ITerrainSystem {
    public:
        FakeTerrainSystem()
        {
            meta.tileSize   = 16;
            meta.resolution = 1.f;
            field = std::make_unique<FakeField>(meta);
        }

        bool coverage = true;

        const ITerrainField &GetField() const override { return *field; }
        const TerrainMeta &GetMeta() const override { return meta; }
        const TerrainTileManifest &GetManifest() const override { return manifest; }
        uint32_t GetLoadedTileCount() const override { return 0; }
        bool SampleRegionLod0(const AABB &, ITerrainRegionSink &) const override { return coverage; }

        void AddChangeListener(ITerrainChangeListener *listener) override
        {
            if (listener != nullptr) {
                listeners.push_back(listener);
            }
        }
        void RemoveChangeListener(ITerrainChangeListener *listener) override
        {
            listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
        }
        void NotifyTilesChanged(const std::vector<TerrainTileCoord> &coords) override
        {
            for (auto *listener : listeners) {
                listener->OnTerrainTilesChanged(coords);
            }
        }

    private:
        TerrainMeta         meta;
        TerrainTileManifest manifest;
        std::unique_ptr<FakeField> field;
        std::vector<ITerrainChangeListener *> listeners;
    };

    class FakeGenerator : public NaviMeshGenerator {
    public:
        void Setup(const WorldPtr &) override {}
        NaviMeshBuildParams GetBuildParams() const override { return params; }
        void CollectTiles(NaviMeshData &) const override {}

    protected:
        bool DoWork() override { return true; }

    private:
        NaviMeshBuildParams params;
    };

    class FakeFactory : public NaviMeshFactory::Impl {
    public:
        NaviMesh *CreateNaviMesh() override { return nullptr; }
        NaviMeshGenerator *CreateGenerator() override { lastGenerator = new FakeGenerator(); return lastGenerator; }
        NaviQueryFilter *CreateQueryFilter() override { return nullptr; }

        FakeGenerator *lastGenerator = nullptr;
    };

    NaviMeshBuildParams MakeNavParams()
    {
        NaviMeshBuildParams params;
        params.resolution.tileSize = 10.f;
        params.bounds = AABB(Vector3(0.f, -100.f, 0.f), Vector3(1000.f, 100.f, 1000.f));
        return params;
    }

} // namespace

TEST(TerrainNavRebuildTest, RebuildsMappedTiles)
{
    FakeTerrainSystem terrain;
    auto *factory = new FakeFactory();
    NaviMeshFactory::Get()->Register(factory);

    TerrainNavRebuildCoordinator coordinator;
    coordinator.Setup(&terrain, MakeNavParams(), nullptr);

    // 16m terrain tile -> 2x2 nav tiles (10m) -> 4 tiles.
    terrain.NotifyTilesChanged({{0, 0}});
    EXPECT_EQ(coordinator.GetPendingCount(), 4u);

    const uint32_t built = coordinator.Update();
    EXPECT_EQ(built, 4u);
    EXPECT_EQ(coordinator.GetPendingCount(), 0u);

    ASSERT_EQ(coordinator.GetLastBuiltTiles().size(), 4u);
    EXPECT_EQ(coordinator.GetLastBuiltTiles()[0].x, 0);
    EXPECT_EQ(coordinator.GetLastBuiltTiles()[0].y, 0);
    EXPECT_EQ(coordinator.GetLastBuiltTiles()[3].x, 1);
    EXPECT_EQ(coordinator.GetLastBuiltTiles()[3].y, 1);

    ASSERT_NE(factory->lastGenerator, nullptr);
    EXPECT_EQ(factory->lastGenerator->GetRebuildTiles().size(), 4u);
    EXPECT_EQ(factory->lastGenerator->GetExportMode(), NaviMeshExportMode::Tiled);

    coordinator.Shutdown();
    NaviMeshFactory::Get()->UnRegister();
}

TEST(TerrainNavRebuildTest, DefersWhenLod0Missing)
{
    FakeTerrainSystem terrain;
    terrain.coverage = false;

    auto *factory = new FakeFactory();
    NaviMeshFactory::Get()->Register(factory);

    TerrainNavRebuildCoordinator coordinator;
    coordinator.Setup(&terrain, MakeNavParams(), nullptr);

    terrain.NotifyTilesChanged({{0, 0}});
    EXPECT_EQ(coordinator.Update(), 0u);
    EXPECT_EQ(coordinator.GetPendingCount(), 4u);   // still queued

    terrain.coverage = true;
    EXPECT_EQ(coordinator.Update(), 4u);
    EXPECT_EQ(coordinator.GetPendingCount(), 0u);

    coordinator.Shutdown();
    NaviMeshFactory::Get()->UnRegister();
}

TEST(TerrainNavRebuildTest, DeterministicOrder)
{
    FakeTerrainSystem terrain;
    auto *factory = new FakeFactory();
    NaviMeshFactory::Get()->Register(factory);

    TerrainNavRebuildCoordinator coordinator;
    coordinator.Setup(&terrain, MakeNavParams(), nullptr);

    terrain.NotifyTilesChanged({{1, 0}, {0, 0}, {0, 1}});
    coordinator.Update();

    const auto first = coordinator.GetLastBuiltTiles();
    // Sorted ascending by (x, y).
    for (size_t i = 1; i < first.size(); ++i) {
        const bool ordered = first[i - 1].x < first[i].x ||
                             (first[i - 1].x == first[i].x && first[i - 1].y <= first[i].y);
        EXPECT_TRUE(ordered);
    }

    coordinator.Shutdown();
    NaviMeshFactory::Get()->UnRegister();
}
