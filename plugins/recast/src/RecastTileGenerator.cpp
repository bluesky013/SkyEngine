//
// Created by blues on 2024/10/8.
//

#include <recast/RecastTileGenerator.h>

#include <recast/RecastLz4Compressor.h>

#include <core/logger/Logger.h>

#include <cmath>
#include <cstring>
#include <limits>

#include <DetourCommon.h>
#include <DetourTileCacheBuilder.h>
#include <Recast.h>

static const char *TAG = "Recast";

namespace sky::ai {

    struct RecastTileBuildContext : public rcContext {
    public:
        RecastTileBuildContext()           = default;
        ~RecastTileBuildContext() override = default;

        void doLog(const rcLogCategory category, const char *msg, const int len) override
        {
            if (category == rcLogCategory::RC_LOG_ERROR) {
                LOG_E(TAG, "%.*s", len, msg);
            } else if (category == rcLogCategory::RC_LOG_WARNING) {
                LOG_W(TAG, "%.*s", len, msg);
            } else {
                LOG_I(TAG, "%.*s", len, msg);
            }
        }

        std::vector<RecastMeshTileData> navigationData;
    };

    RecastRasterizeContext::~RecastRasterizeContext()
    {
        rcFreeHeightField(solidHF);
        rcFreeHeightfieldLayerSet(layerSet);
        rcFreeCompactHeightfield(compactHF);
    }

    RecastTileCacheContext::~RecastTileCacheContext()
    {
        dtFreeTileCacheLayer(allocator, cacheLayer);
        dtFreeTileCacheContourSet(allocator, contourSet);
        dtFreeTileCachePolyMesh(allocator, polyMesh);
    }

    RecastTileGenerator::RecastTileGenerator(const rcConfig &cfg, const RecastTileBuildParam &param) : config(cfg), buildParam(param)
    {
        const float tcs = static_cast<float>(cfg.tileSize) * cfg.cs;
        auto        tx  = static_cast<float>(param.coord.x);
        auto        ty  = static_cast<float>(param.coord.y);

        config.bmin[0] = cfg.bmin[0] + tx * tcs;
        config.bmin[1] = cfg.bmin[1];
        config.bmin[2] = cfg.bmin[2] + ty * tcs;

        config.bmax[0] = cfg.bmin[0] + (tx + 1) * tcs;
        config.bmax[1] = cfg.bmax[1];
        config.bmax[2] = cfg.bmin[2] + (ty + 1) * tcs;

        config.bmin[0] -= static_cast<float>(cfg.borderSize) * cfg.cs;
        config.bmin[2] -= static_cast<float>(cfg.borderSize) * cfg.cs;

        config.bmax[0] += static_cast<float>(cfg.borderSize) * cfg.cs;
        config.bmax[2] += static_cast<float>(cfg.borderSize) * cfg.cs;
    }

    void RecastTileGenerator::Setup(const std::shared_ptr<NaviOctree> &inOctree)
    {
        octree = inOctree;
    }

    bool RecastTileGenerator::DoWork()
    {
        const bool result = octree != nullptr && GenerateTile();
        succeeded.store(result);
        return result;
    }

    bool RecastTileGenerator::GenerateTile()
    {
        RecastTileBuildContext context;
        if (!GenerateCompressedLayers(context)) {
            return false;
        }

        data.swap(context.navigationData);
        return true;
    }

    bool RecastTileGenerator::GenerateCompressedLayers(RecastTileBuildContext &context)
    {
        RecastRasterizeContext rasterizeContext;

        if (!CreateHeightField(context, rasterizeContext)) {
            return false;
        }

        if (!RasterizeTriangles(context, rasterizeContext)) {
            return false;
        }

        FilterProcess(context, rasterizeContext);
        if (!CreateCompactHeightField(context, rasterizeContext)) {
            return false;
        }

        if (!ErodeWalkableArea(context, rasterizeContext)) {
            return false;
        }

        if (!BuildLayers(context, rasterizeContext)) {
            return false;
        }

        return BuildTileCache(context, rasterizeContext);
    }

    bool RecastTileGenerator::RasterizeTriangles(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext)
    {
        return RasterizeGeometry(buildCtx, rasterizeContext);
    }

    bool RecastTileGenerator::RasterizeGeometry(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext)
    {
        const AABB bound(Vector3(config.bmin[0], config.bmin[1], config.bmin[2]), Vector3(config.bmax[0], config.bmax[1], config.bmax[2]));
        bool       valid = true;
        octree->ForeachWithBoundTest(bound, [this, &valid, &buildCtx, &rasterizeContext](const NaviOctreeElementRef &element) {
            const auto  &mesh        = *element->triangleMesh;
            const auto  &view        = mesh.views[element->viewIndex];
            const size_t indexStride = mesh.indexType == IndexType::U32 ? sizeof(uint32_t) : sizeof(uint16_t);
            if (mesh.vtxStride < sizeof(Vector3) || view.numVert > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
                view.numTris > static_cast<uint32_t>(std::numeric_limits<int>::max() / 3) ||
                (static_cast<size_t>(view.firstVertex) + view.numVert) * mesh.vtxStride > mesh.position.size() ||
                (static_cast<size_t>(view.firstIndex) + static_cast<size_t>(view.numTris) * 3) * indexStride > mesh.indexRaw.size()) {
                valid = false;
                return;
            }
            std::vector<Vector3> vertices(view.numVert);
            for (uint32_t i = 0; i < view.numVert; ++i) {
                std::memcpy(&vertices[i], mesh.position.data() + (static_cast<size_t>(view.firstVertex) + i) * mesh.vtxStride, sizeof(Vector3));
                for (int axis = 0; axis < 3; ++axis) {
                    if (!std::isfinite(vertices[i].v[axis])) {
                        valid = false;
                        return;
                    }
                }
            }
            std::vector<int> indices(static_cast<size_t>(view.numTris) * 3);
            for (size_t i = 0; i < indices.size(); ++i) {
                uint32_t    index = 0;
                const auto *bytes = mesh.indexRaw.data() + (static_cast<size_t>(view.firstIndex) + i) * indexStride;
                if (mesh.indexType == IndexType::U32) {
                    std::memcpy(&index, bytes, sizeof(index));
                } else {
                    uint16_t shortIndex = 0;
                    std::memcpy(&shortIndex, bytes, sizeof(shortIndex));
                    index = shortIndex;
                }
                if (index >= view.numVert) {
                    valid = false;
                    return;
                }
                indices[i] = static_cast<int>(index);
            }
            if (vertices.empty() || indices.empty()) {
                return;
            }
            triAreas.assign(view.numTris, 0);
            rcMarkWalkableTriangles(&buildCtx, config.walkableSlopeAngle, vertices.front().v, static_cast<int>(vertices.size()), indices.data(),
                                    static_cast<int>(view.numTris), triAreas.data());
            if (!rcRasterizeTriangles(&buildCtx, vertices.front().v, static_cast<int>(vertices.size()), indices.data(), triAreas.data(),
                                      static_cast<int>(view.numTris), *rasterizeContext.solidHF, config.walkableClimb)) {
                valid = false;
            }
        });
        return valid;
    }

    bool RecastTileGenerator::BuildTileCache(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext)
    {
        auto layers = rasterizeContext.layerSet->nlayers;
        rasterizeContext.layerData.resize(layers);
        for (int i = 0; i < layers; ++i) {
            auto                     &tileData = rasterizeContext.layerData[i];
            const rcHeightfieldLayer *layer    = &rasterizeContext.layerSet->layers[i];

            dtTileCacheLayerHeader header{};
            header.magic   = DT_TILECACHE_MAGIC;
            header.version = DT_TILECACHE_VERSION;

            header.tx     = buildParam.coord.x;
            header.ty     = buildParam.coord.y;
            header.tlayer = i;

            dtVcopy(header.bmin, layer->bmin);
            dtVcopy(header.bmax, layer->bmax);

            header.width  = static_cast<uint8_t>(layer->width);
            header.height = static_cast<uint8_t>(layer->height);
            header.minx   = static_cast<uint8_t>(layer->minx);
            header.maxx   = static_cast<uint8_t>(layer->maxx);
            header.miny   = static_cast<uint8_t>(layer->miny);
            header.maxy   = static_cast<uint8_t>(layer->maxy);

            header.hmin = static_cast<uint16_t>(layer->hmin);
            header.hmax = static_cast<uint16_t>(layer->hmax);

            uint8_t *outData = nullptr;
            int32_t  outSize = 0;

            auto status = dtBuildTileCacheLayer(&compressor, &header, layer->heights, layer->areas, layer->cons, &outData, &outSize);
            if (dtStatusFailed(status)) {
                dtFree(outData);
                buildCtx.log(RC_LOG_ERROR, "Build tile cache layer failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
                return false;
            }

            auto *navData    = new RecastNavData();
            navData->data    = outData;
            navData->size    = static_cast<uint32_t>(outSize);
            tileData.navData = navData;

            buildCtx.log(RC_LOG_PROGRESS, "Build tile cache layer. %d, %d, %d", buildParam.coord.x, buildParam.coord.y, i);
        }

        if (layers == 0) {
            LOG_I(TAG, "build tile no layer %d, %d, min[%f, %f, %f], max[%f, %f, %f], %d, %d", buildParam.coord.x, buildParam.coord.y, config.bmin[0],
                  config.bmin[1], config.bmin[2], config.bmax[0], config.bmax[1], config.bmax[2], config.width, config.height);
        }

        buildCtx.navigationData.swap(rasterizeContext.layerData);
        return true;
    }

    bool RecastTileGenerator::BuildLayers(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        auto *layerSet = rcAllocHeightfieldLayerSet();
        if (layerSet == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate height field layer set failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }

        rasterizeContext.layerSet = layerSet;
        if (!rcBuildHeightfieldLayers(&buildCtx, *rasterizeContext.compactHF, config.borderSize, config.walkableHeight, *layerSet)) {
            buildCtx.log(RC_LOG_ERROR, "Build height field layers failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }
        return true;
    }

    bool RecastTileGenerator::ErodeWalkableArea(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        if (!rcErodeWalkableArea(&buildCtx, config.walkableRadius, *rasterizeContext.compactHF)) {
            return false; // NOLINT
        }

        return true;
    }

    void RecastTileGenerator::FilterProcess(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        rcFilterLowHangingWalkableObstacles(&buildCtx, config.walkableClimb, *rasterizeContext.solidHF);
        rcFilterLedgeSpans(&buildCtx, config.walkableHeight, config.walkableClimb, *rasterizeContext.solidHF);
        rcFilterWalkableLowHeightSpans(&buildCtx, config.walkableHeight, *rasterizeContext.solidHF);
    }

    bool RecastTileGenerator::CreateCompactHeightField(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        auto *chf = rcAllocCompactHeightfield();
        if (chf == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate compact height field failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }

        rasterizeContext.compactHF = chf;
        if (!rcBuildCompactHeightfield(&buildCtx, config.walkableHeight, config.walkableClimb, *rasterizeContext.solidHF, *chf)) {
            buildCtx.log(RC_LOG_ERROR, "Build compact data failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }
        return true;
    }

    bool RecastTileGenerator::CreateHeightField(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        auto *hf = rcAllocHeightfield();
        if (hf == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate height field failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }

        rasterizeContext.solidHF = hf;
        if (!rcCreateHeightfield(&buildCtx, *hf, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch)) {
            buildCtx.log(RC_LOG_ERROR, "Create height field failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }
        return true;
    }
} // namespace sky::ai