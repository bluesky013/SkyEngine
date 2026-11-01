//
// Created by blues on 2024/9/6.
//

#include <recast/RecastNaviMeshGenerator.h>

#include <recast/RecastConversion.h>

#include <core/math/MathUtil.h>
#include <framework/compression/Compressor.h>
#include <navigation/INavigationSystem.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <physics/IPhysicsSystem.h>

namespace sky::ai {
    namespace {

        // Feeds provider triangles into the nav octree as engine triangle meshes.
        class OctreeTriangleSink : public INaviGeometrySink {
        public:
            explicit OctreeTriangleSink(NaviOctree *inOctree) : octree(inOctree)
            {
            }

            void AddTriangles(const Vector3 *vertices, uint32_t vertexCount, const uint32_t *indices, uint32_t indexCount) override
            {
                if (vertices == nullptr || indices == nullptr || vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0) {
                    valid = false;
                    return;
                }
                for (uint32_t i = 0; i < vertexCount; ++i) {
                    for (int axis = 0; axis < 3; ++axis) {
                        if (!std::isfinite(vertices[i].v[axis])) {
                            valid = false;
                            return;
                        }
                    }
                }
                for (uint32_t i = 0; i < indexCount; ++i) {
                    if (indices[i] >= vertexCount) {
                        valid = false;
                        return;
                    }
                }

                auto *mesh = new TriangleMesh();
                mesh->position.resize(static_cast<size_t>(vertexCount) * sizeof(Vector3));
                std::memcpy(mesh->position.data(), vertices, mesh->position.size());

                mesh->indexRaw.resize(static_cast<size_t>(indexCount) * sizeof(uint32_t));
                std::memcpy(mesh->indexRaw.data(), indices, mesh->indexRaw.size());
                mesh->indexType = IndexType::U32;
                mesh->vtxStride = sizeof(Vector3);

                AABB box;
                box.min = vertices[0];
                box.max = vertices[0];
                for (uint32_t i = 1; i < vertexCount; ++i) {
                    box.min = Vector3(std::min(box.min.x, vertices[i].x), std::min(box.min.y, vertices[i].y), std::min(box.min.z, vertices[i].z));
                    box.max = Vector3(std::max(box.max.x, vertices[i].x), std::max(box.max.y, vertices[i].y), std::max(box.max.z, vertices[i].z));
                }
                mesh->AddView(0, vertexCount, 0, indexCount, box);

                auto *element         = new NaviOctreeElement();
                element->triangleMesh = mesh;
                element->viewIndex    = 0;
                octree->AddElement(element);
            }

            bool IsValid() const
            {
                return valid;
            }

        private:
            NaviOctree *octree;
            bool        valid = true;
        };

    } // namespace

    void RecastNaviMeshGenerator::Setup(const WorldPtr &inWorld)
    {
        if (IsWorking()) {
            return;
        }
        world   = inWorld;
        navMesh = nullptr;
        ready   = false;
        succeeded.store(false);
        error.clear();
        tileGenerators.clear();
        pendingTiles.clear();
        exportData = {};
        config     = {};
        if (!world) {
            error = "Navigation build requires a world";
            return;
        }
        auto *navSys = static_cast<INavigationSystem *>(world->GetSubSystem(Name(INavigationSystem::NAME.data())));
        if (navSys == nullptr || navSys->GetNaviMesh() == nullptr) {
            error = "Navigation world subsystem or mesh is unavailable";
            return;
        }
        navMesh                            = navSys->GetNaviMesh();
        buildParams.agent                  = navMesh->GetAgentConfig();
        buildParams.resolution             = navMesh->GetResolution();
        buildParams.bounds                 = navMesh->GetBounds();
        buildParams.maxSimplificationError = navMesh->GetMaxSimplificationError();
        const auto &resolution             = buildParams.resolution;
        const auto &agentCfg               = buildParams.agent;
        if (!std::isfinite(resolution.cellSize) || resolution.cellSize <= 0.f || !std::isfinite(resolution.cellHeight) ||
            resolution.cellHeight <= 0.f || !std::isfinite(resolution.tileSize) || resolution.tileSize < resolution.cellSize ||
            resolution.tileSize / resolution.cellSize > 255.f || !std::isfinite(agentCfg.height) || agentCfg.height <= 0.f ||
            !std::isfinite(agentCfg.radius) || agentCfg.radius < 0.f || !std::isfinite(agentCfg.maxClimb) || agentCfg.maxClimb < 0.f ||
            !std::isfinite(agentCfg.maxSlope) || agentCfg.maxSlope < 0.f || agentCfg.maxSlope >= 90.f ||
            !std::isfinite(buildParams.maxSimplificationError) || buildParams.maxSimplificationError < 0.f ||
            agentCfg.height / resolution.cellHeight > 65535.f || agentCfg.radius / resolution.cellSize > 255.f ||
            agentCfg.maxClimb / resolution.cellHeight > 65535.f ||
            12.f / resolution.cellSize > static_cast<float>(std::numeric_limits<int>::max() / 2)) {
            error = "Navigation build parameters are invalid";
            return;
        }
        for (int axis = 0; axis < 3; ++axis) {
            const float low  = buildParams.bounds.min.v[axis];
            const float high = buildParams.bounds.max.v[axis];
            if (!std::isfinite(low) || !std::isfinite(high) || low >= high || !std::isfinite(high - low) ||
                (axis == 1 && (high - low) / resolution.cellHeight > 65535.f) ||
                (high - low) / resolution.cellSize > static_cast<float>(std::numeric_limits<int>::max() / 2)) {
                error = "Navigation build bounds are invalid";
                return;
            }
        }
        config.cs                     = resolution.cellSize;
        config.ch                     = resolution.cellHeight;
        config.walkableSlopeAngle     = agentCfg.maxSlope;
        config.walkableHeight         = CeilTo<int>(agentCfg.height / config.ch);
        config.walkableClimb          = FloorTo<int>(agentCfg.maxClimb / config.ch);
        config.walkableRadius         = CeilTo<int>(agentCfg.radius / config.cs);
        config.tileSize               = FloorTo<int>(resolution.tileSize / config.cs);
        config.borderSize             = config.walkableRadius + 3;
        config.width                  = config.tileSize + config.borderSize * 2;
        config.height                 = config.width;
        config.maxEdgeLen             = static_cast<int>(12.f / config.cs);
        config.maxSimplificationError = buildParams.maxSimplificationError;
        config.minRegionArea          = 8;
        config.mergeRegionArea        = 20;
        config.maxVertsPerPoly        = 6;
        config.detailSampleDist       = 6.f;
        config.detailSampleMaxError   = 1.f;
        ToRecast(buildParams.bounds.min, config.bmin);
        ToRecast(buildParams.bounds.max, config.bmax);
        buildParams.resolution.tileSize = static_cast<float>(config.tileSize) * config.cs;
        buildParams.borderSize          = config.borderSize;
        int gridWidth = 0, gridHeight = 0;
        rcCalcGridSize(config.bmin, config.bmax, config.cs, &gridWidth, &gridHeight);
        const int width  = (gridWidth + config.tileSize - 1) / config.tileSize;
        const int height = (gridHeight + config.tileSize - 1) / config.tileSize;
        if (width <= 0 || height <= 0 || (rebuildTiles.empty() && static_cast<uint64_t>(width) * height > navMesh->GetMaxTiles())) {
            error = "Navigation build exceeds the tile capacity";
            return;
        }
        for (const auto &tile : rebuildTiles) {
            if (tile.x < 0 || tile.x >= width || tile.y < 0 || tile.y >= height) {
                error = "Navigation rebuild tile is outside the build bounds";
                return;
            }
        }
        float extent = 1.f;
        for (int axis = 0; axis < 3; ++axis) {
            extent = std::max(extent, std::max(std::abs(config.bmin[axis]), std::abs(config.bmax[axis])));
        }
        buildOctree = std::make_shared<NaviOctree>(extent * 2.f);
        ready       = true;
    }

    bool RecastNaviMeshGenerator::GatherGeometry(NaviOctree *octree)
    {
        // Collision geometry comes from the physics runtime through an engine interface, so navigation
        // never depends on a physics backend or render module.
        auto *provider = static_cast<phy::IPhysicsSystem *>(world->GetSubSystem(Name(phy::PHYSICS_SYSTEM_NAME.data())));
        if (provider != nullptr) {
            std::vector<phy::CollisionMeshInstance> instances;
            provider->GatherCollisionMeshes(instances);

            for (const auto &instance : instances) {
                if (!instance.mesh) {
                    continue;
                }
                if (instance.mesh->vtxStride < sizeof(Vector3) || instance.mesh->position.size() % instance.mesh->vtxStride != 0) {
                    return false;
                }
                const Matrix4       worldMatrix  = instance.transform.ToMatrix();
                const TriangleMesh *triangleMesh = instance.mesh.Get();

                auto      *scaledTriangleMesh = new TriangleMesh();
                const auto vtxCount           = triangleMesh->position.size() / triangleMesh->vtxStride;
                scaledTriangleMesh->position.resize(vtxCount * sizeof(Vector3));
                scaledTriangleMesh->indexRaw  = triangleMesh->indexRaw;
                scaledTriangleMesh->vtxStride = sizeof(Vector3);
                scaledTriangleMesh->indexType = triangleMesh->indexType;
                scaledTriangleMesh->views     = triangleMesh->views;

                for (size_t i = 0; i < vtxCount; ++i) {
                    Vector3 vertex;
                    std::memcpy(&vertex, triangleMesh->position.data() + i * triangleMesh->vtxStride, sizeof(vertex));
                    const auto transformed = instance.transform.Translate(vertex);
                    std::memcpy(scaledTriangleMesh->position.data() + i * sizeof(Vector3), &transformed, sizeof(transformed));
                }

                for (uint32_t i = 0; i < triangleMesh->views.size(); ++i) {
                    scaledTriangleMesh->views[i].aabb = AABB::Transform(triangleMesh->views[i].aabb, worldMatrix);

                    auto *element         = new NaviOctreeElement();
                    element->triangleMesh = scaledTriangleMesh;
                    element->viewIndex    = i;

                    octree->AddElement(element);
                }
            }
        }

        // Providers (e.g. terrain) contribute additional geometry sources without the backend linking them.
        auto *navSys = static_cast<INavigationSystem *>(world->GetSubSystem(Name(INavigationSystem::NAME.data())));
        if (navSys != nullptr && !navSys->GetGeometryProviders().empty()) {
            std::vector<AABB> regions;
            if (rebuildTiles.empty()) {
                regions.emplace_back(buildParams.bounds);
            } else {
                const float border   = static_cast<float>(config.borderSize) * config.cs;
                const float tileSize = buildParams.resolution.tileSize;
                for (const auto &tile : rebuildTiles) {
                    const float x = config.bmin[0] + static_cast<float>(tile.x) * tileSize;
                    const float z = config.bmin[2] + static_cast<float>(tile.y) * tileSize;
                    regions.emplace_back(
                        Vector3(std::max(config.bmin[0], x - border), config.bmin[1], std::max(config.bmin[2], z - border)),
                        Vector3(std::min(config.bmax[0], x + tileSize + border), config.bmax[1], std::min(config.bmax[2], z + tileSize + border)));
                }
            }
            OctreeTriangleSink sink(octree);
            for (auto *geometryProvider : navSys->GetGeometryProviders()) {
                for (const auto &region : regions) {
                    if (geometryProvider == nullptr || !geometryProvider->Collect(region, sink) || !sink.IsValid()) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    void RecastNaviMeshGenerator::PrepareTiles(std::vector<RecastTile> &tiles) const
    {
        if (!rebuildTiles.empty()) {
            tiles.reserve(rebuildTiles.size());
            for (const auto &coord : rebuildTiles) {
                tiles.emplace_back(RecastTile{coord.x, coord.y});
            }
            return;
        }

        const auto &min = config.bmin;
        const auto &max = config.bmax;

        int gw = 0;
        int gh = 0;

        rcCalcGridSize(min, max, config.cs, &gw, &gh);

        const int ts = config.tileSize;
        const int tw = (gw + ts - 1) / ts;
        const int th = (gh + ts - 1) / ts;

        for (int i = 0; i < tw; ++i) {
            for (int j = 0; j < th; ++j) {
                tiles.emplace_back(RecastTile{i, j});
            }
        }
    }

    void RecastNaviMeshGenerator::PrepareWork()
    {
        succeeded.store(false);
        if (!ready) {
            world = nullptr;
            return;
        }
        if (!CompressionManager::Get()->AcquireCompressor(CompressionMethod::LZ4)) {
            ready = false;
            error = "Navigation tile generation requires an LZ4 compression provider";
            world = nullptr;
            return;
        }
        const bool complete = GatherGeometry(buildOctree.get());
        world               = nullptr;
        if (!complete) {
            ready = false;
            error = "Navigation geometry is invalid or incomplete";
            return;
        }
        PrepareTiles(pendingTiles);
        if (pendingTiles.size() > navMesh->GetMaxTiles()) {
            ready = false;
            error = "Navigation build exceeds the tile capacity";
            return;
        }

        for (auto &tileCoord : pendingTiles) {
            RecastTileBuildParam param = {};
            param.coord                = tileCoord;

            CounterPtr<RecastTileGenerator> generator = new RecastTileGenerator(config, param);
            generator->Setup(buildOctree);
            generator->StartAsync();

            dependencies.emplace_back(generator->GetTask());
            tileGenerators.emplace_back(generator);
        }
    }

    NaviMeshBuildParams RecastNaviMeshGenerator::GetBuildParams() const
    {
        return buildParams;
    }

    void RecastNaviMeshGenerator::CollectTiles(NaviMeshData &out) const
    {
        out = exportData;
    }

    void RecastNaviMeshGenerator::SnapshotTiles(NaviMeshData &out) const
    {
        out.mode   = NaviMeshExportMode::Tiled;
        out.params = GetBuildParams();
        out.tiles.clear();

        for (const auto &generator : tileGenerators) {
            const auto &coord    = generator->GetParam().coord;
            const auto &tileData = generator->GetData();

            for (uint32_t layer = 0; layer < static_cast<uint32_t>(tileData.size()); ++layer) {
                const auto &tile = tileData[layer];
                if (tile.navData == nullptr || tile.navData->size == 0) {
                    continue;
                }

                NaviMeshTilePayload payload;
                payload.tx    = coord.x;
                payload.ty    = coord.y;
                payload.layer = layer;
                payload.data.assign(tile.navData->data, tile.navData->data + tile.navData->size);
                out.tiles.emplace_back(std::move(payload));
            }
        }
    }

    bool RecastNaviMeshGenerator::DoWork()
    {
        if (!ready) {
            return false;
        }
        for (const auto &generator : tileGenerators) {
            if (!generator->HasSucceeded()) {
                error = "Navigation tile generation failed";
                return false;
            }
        }
        SnapshotTiles(exportData);
        if (!navMesh->ApplyBuild(exportData, rebuildTiles)) {
            error      = "Navigation tile installation failed";
            exportData = {};
            return false;
        }
        if (exportMode == NaviMeshExportMode::Full) {
            exportData.mode = NaviMeshExportMode::Full;
            if (!navMesh->SaveFullData(exportData.fullData)) {
                error      = "Navigation full mesh serialization failed";
                exportData = {};
                return false;
            }
            exportData.tiles.clear();
        }
        succeeded.store(true);
        return true;
    }
} // namespace sky::ai
