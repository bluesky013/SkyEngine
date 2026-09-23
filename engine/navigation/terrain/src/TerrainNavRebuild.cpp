//
// Created on 2026/09/23.
//

#include <navigation/terrain/TerrainNavRebuild.h>

#include <navigation/NaviMeshFactory.h>

#include <terrain/TerrainAddress.h>
#include <terrain/TerrainQuery.h>
#include <terrain/TerrainSystemInterface.h>

#include <algorithm>
#include <cmath>

namespace sky::ai {

    namespace {

        inline uint64_t MakeNavTileKey(int32_t x, int32_t y)
        {
            return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
                   static_cast<uint64_t>(static_cast<uint32_t>(y));
        }

        // Coverage probe: the region sink is a no-op; only the returned bool matters.
        class NullRegionSink : public terrain::ITerrainRegionSink {
        public:
            void OnTerrainTileLod0(const terrain::TerrainTileCoord &, const terrain::TerrainMeta &,
                                   const float *, uint32_t) override {}
        };

    } // namespace

    TerrainNavRebuildCoordinator::~TerrainNavRebuildCoordinator()
    {
        Shutdown();
    }

    void TerrainNavRebuildCoordinator::Setup(terrain::ITerrainSystem *inTerrain, const NaviMeshBuildParams &inParams,
                                             const WorldPtr &inWorld)
    {
        Shutdown();
        terrain   = inTerrain;
        navParams = inParams;
        world     = inWorld;
        if (terrain != nullptr) {
            terrain->AddChangeListener(this);
        }
    }

    void TerrainNavRebuildCoordinator::Shutdown()
    {
        if (terrain != nullptr) {
            terrain->RemoveChangeListener(this);
            terrain = nullptr;
        }
        pendingTiles.clear();
        lastBuiltTiles.clear();
    }

    std::vector<NaviMeshTileCoord> TerrainNavRebuildCoordinator::MapTerrainTile(const terrain::TerrainTileCoord &coord) const
    {
        std::vector<NaviMeshTileCoord> tiles;
        if (terrain == nullptr) {
            return tiles;
        }

        const terrain::TerrainMeta &meta = terrain->GetMeta();
        const float navTile = navParams.resolution.tileSize;
        if (navTile <= 0.f) {
            return tiles;
        }

        const Vector3 origin = terrain::TileToWorld(meta, coord);
        const float   size   = meta.GetTileWorldSize();

        const int32_t minX = static_cast<int32_t>(std::floor((origin.x - navParams.bounds.min.x) / navTile));
        const int32_t maxX = static_cast<int32_t>(std::floor((origin.x + size - navParams.bounds.min.x) / navTile));
        const int32_t minY = static_cast<int32_t>(std::floor((origin.z - navParams.bounds.min.z) / navTile));
        const int32_t maxY = static_cast<int32_t>(std::floor((origin.z + size - navParams.bounds.min.z) / navTile));

        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t x = minX; x <= maxX; ++x) {
                tiles.push_back(NaviMeshTileCoord{x, y});
            }
        }
        return tiles;
    }

    bool TerrainNavRebuildCoordinator::HasLod0Coverage(const NaviMeshTileCoord &tile) const
    {
        if (terrain == nullptr) {
            return false;
        }

        const float navTile = navParams.resolution.tileSize;
        const float x0 = navParams.bounds.min.x + static_cast<float>(tile.x) * navTile;
        const float z0 = navParams.bounds.min.z + static_cast<float>(tile.y) * navTile;

        const AABB region(Vector3(x0, navParams.bounds.min.y, z0),
                          Vector3(x0 + navTile, navParams.bounds.max.y, z0 + navTile));

        NullRegionSink sink;
        return terrain->SampleRegionLod0(region, sink);
    }

    void TerrainNavRebuildCoordinator::OnTerrainTilesChanged(const std::vector<terrain::TerrainTileCoord> &coords)
    {
        for (const auto &coord : coords) {
            for (const auto &tile : MapTerrainTile(coord)) {
                pendingTiles.insert(MakeNavTileKey(tile.x, tile.y));
            }
        }
    }

    uint32_t TerrainNavRebuildCoordinator::Update()
    {
        lastBuiltTiles.clear();
        if (pendingTiles.empty()) {
            return 0;
        }

        std::vector<NaviMeshTileCoord> ordered;
        ordered.reserve(pendingTiles.size());
        for (const uint64_t key : pendingTiles) {
            ordered.push_back(NaviMeshTileCoord{static_cast<int32_t>(key >> 32), static_cast<int32_t>(key & 0xffffffffu)});
        }
        std::sort(ordered.begin(), ordered.end(), [](const NaviMeshTileCoord &a, const NaviMeshTileCoord &b) {
            return a.x != b.x ? a.x < b.x : a.y < b.y;
        });

        // Only rebuild tiles whose terrain LOD0 is resident; others stay queued (deferred).
        std::vector<NaviMeshTileCoord> ready;
        for (const auto &tile : ordered) {
            if (HasLod0Coverage(tile)) {
                ready.push_back(tile);
            }
        }
        if (ready.empty()) {
            return 0;
        }

        auto generator = NaviMeshFactory::Get()->CreateGenerator();
        if (generator == nullptr) {
            return 0;
        }

        generator->SetExportMode(NaviMeshExportMode::Tiled);
        generator->SetRebuildTiles(ready);
        generator->Setup(world);
        generator->StartAsync();

        for (const auto &tile : ready) {
            pendingTiles.erase(MakeNavTileKey(tile.x, tile.y));
        }
        lastBuiltTiles = ready;
        return static_cast<uint32_t>(ready.size());
    }

} // namespace sky::ai
