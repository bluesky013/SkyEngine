//
// Created by blues on 2024/10/8.
//

#include <recast/RecastTileGenerator.h>
#include <recast/RecastConversion.h>
#include <recast/RecastLz4Compressor.h>

#include <core/logger/Logger.h>

#include <Recast.h>
#include <DetourTileCacheBuilder.h>
#include <DetourCommon.h>

static const char* TAG = "Recast";

namespace sky::ai {

    struct RecastTileBuildContext : public rcContext {
    public:
        RecastTileBuildContext() = default;
        ~RecastTileBuildContext() override = default;

        void doLog(const rcLogCategory category, const char* msg, const int len) override
        {
            if (category == rcLogCategory::RC_LOG_ERROR) {
                LOG_E(TAG, msg);
            } else if (category == rcLogCategory::RC_LOG_WARNING) {
                LOG_W(TAG, msg);
            } else {
                LOG_I(TAG, msg);
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

    RecastTileGenerator::RecastTileGenerator(const rcConfig &cfg, const RecastTileBuildParam &param)
        : config(cfg)
        , buildParam(param)
    {
        const float tcs = static_cast<float>(cfg.tileSize) * cfg.cs;
        auto tx = static_cast<float>(param.coord.x);
        auto ty = static_cast<float>(param.coord.y);

        config.bmin[0] = tx * tcs;
        config.bmin[1] = cfg.bmin[1];
        config.bmin[2] = ty * tcs;

        config.bmax[0] = (tx + 1) * tcs;
        config.bmax[1] = cfg.bmax[1];
        config.bmax[2] = (ty + 1) * tcs;

        config.bmin[0] -= static_cast<float>(cfg.borderSize) * cfg.cs;
        config.bmin[2] -= static_cast<float>(cfg.borderSize) * cfg.cs;

        config.bmax[0] += static_cast<float>(cfg.borderSize) * cfg.cs;
        config.bmax[2] += static_cast<float>(cfg.borderSize) * cfg.cs;
    }

    void RecastTileGenerator::Setup(const CounterPtr<RecastNaviMesh> &mesh)
    {
        navMesh = mesh;
    }

    bool RecastTileGenerator::DoWork()
    {
        GenerateTile();
        return true;
    }

    void RecastTileGenerator::GenerateTile()
    {
        RecastTileBuildContext context;
        if (!GenerateCompressedLayers(context)) {
            return;
        }

        data.swap(context.navigationData);
    }

    bool RecastTileGenerator::GenerateCompressedLayers(RecastTileBuildContext &context)
    {
        RecastRasterizeContext rasterizeContext;

        if (!CreateHeightField(context, rasterizeContext)) {
            return false;
        }

        RasterizeTriangles(context, rasterizeContext);

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

    void RecastTileGenerator::RasterizeTriangles(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext)
    {
        RasterizeGeometry(buildCtx, rasterizeContext);
    }

    void RecastTileGenerator::RasterizeGeometry(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext)
    {
        auto *octree = navMesh->GetOctree();

        AABB bound = {};
        bound.min.x = static_cast<float>(buildParam.coord.x * config.tileSize) * config.cs;
        bound.min.z = static_cast<float>(buildParam.coord.y * config.tileSize) * config.cs;
        bound.min.y = std::numeric_limits<float>::lowest();

        bound.max.x = static_cast<float>((buildParam.coord.x + 1) * config.tileSize) * config.cs;
        bound.max.z = static_cast<float>((buildParam.coord.y + 1) * config.tileSize) * config.cs;
        bound.max.y = std::numeric_limits<float>::max();

        octree->ForeachWithBoundTest(bound, [this, &buildCtx, &rasterizeContext](const NaviOctreeElementRef &mesh) {
            auto &meshView = mesh->triangleMesh->views[mesh->viewIndex];
            const auto* vertices = &reinterpret_cast<const float*>(mesh->triangleMesh->position.data())[meshView.firstVertex];
            int numVertices = static_cast<int>(meshView.numVert);

            SKY_ASSERT(mesh->triangleMesh->indexType == IndexType::U32)
            const int* tris = &reinterpret_cast<const int*>(mesh->triangleMesh->indexRaw.data())[meshView.firstIndex];
            int numTris = static_cast<int>(meshView.numTris);

            triAreas.resize(numTris);
            rcMarkWalkableTriangles(&buildCtx, config.walkableSlopeAngle, vertices, numVertices, tris, numTris, triAreas.data());
            if (!rcRasterizeTriangles(&buildCtx, vertices, numVertices, tris, triAreas.data(), numTris, *rasterizeContext.solidHF, config.walkableClimb)) {
                buildCtx.log(RC_LOG_ERROR, "Rasterize triangle failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            }
        });
    }

    bool RecastTileGenerator::BuildTileCache(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        auto layers = rasterizeContext.layerSet->nlayers;
        rasterizeContext.layerData.resize(layers);
        for (int i = 0; i < layers; ++i) {
            auto &tileData = rasterizeContext.layerData[i];
            const rcHeightfieldLayer* layer = &rasterizeContext.layerSet->layers[i];

            dtTileCacheLayerHeader header {};
            header.magic   = DT_TILECACHE_MAGIC;
            header.version = DT_TILECACHE_VERSION;

            header.tx = buildParam.coord.x;
            header.ty = buildParam.coord.y;
            header.tlayer = i;

            dtVcopy(header.bmin, layer->bmin);
            dtVcopy(header.bmax, layer->bmax);

            header.width  = static_cast<uint8_t>(layer->width);
            header.height = static_cast<uint8_t>(layer->height);
            header.minx   = static_cast<uint8_t>(layer->minx);
            header.maxx   = static_cast<uint8_t>(layer->maxx);
            header.miny   = static_cast<uint8_t>(layer->miny);
            header.maxy   = static_cast<uint8_t>(layer->maxy);

            header.hmin   = static_cast<uint16_t>(layer->hmin);
            header.hmax   = static_cast<uint16_t>(layer->hmax);

            uint8_t *outData = nullptr;
            int32_t outSize = 0;

            auto *compressor = GetOrCreateCompressor();

            auto status = dtBuildTileCacheLayer(compressor, &header, layer->heights, layer->areas, layer->cons, &outData, &outSize);
            if (dtStatusFailed(status)) {
                dtFree(outData);
                buildCtx.log(RC_LOG_ERROR, "Build tile cache layer failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
                return false;
            }

            // From UE version, outData allocates a lots of space
            auto *navData = new RecastNavData();
            navData->data = reinterpret_cast<uint8_t*>(dtAlloc(outSize * sizeof(uint8_t), DT_ALLOC_PERM));
            navData->size = static_cast<uint32_t>(outSize);
            tileData.navData = navData;

            memcpy(navData->data, outData, outSize);
            dtFree(outData);

            buildCtx.log(RC_LOG_PROGRESS, "Build tile cache layer. %d, %d, %d", buildParam.coord.x, buildParam.coord.y, i);
        }

        if (layers == 0) {
            LOG_I(TAG, "build tile no layer %d, %d, min[%f, %f, %f], max[%f, %f, %f], %d, %d", buildParam.coord.x, buildParam.coord.y,
                  config.bmin[0], config.bmin[1], config.bmin[2],
                  config.bmax[0], config.bmax[1], config.bmax[2],
                  config.width, config.height
            );
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

        if (!rcBuildHeightfieldLayers(&buildCtx, *rasterizeContext.compactHF, config.borderSize, config.walkableHeight, *layerSet)) {
            buildCtx.log(RC_LOG_ERROR, "Build height field layers failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }
        rasterizeContext.layerSet = layerSet;

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

        if (!rcBuildCompactHeightfield(&buildCtx, config.walkableHeight, config.walkableClimb, *rasterizeContext.solidHF, *chf))
        {
            buildCtx.log(RC_LOG_ERROR, "Build compact data failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }
        rasterizeContext.compactHF = chf;
        return true;
    }

    bool RecastTileGenerator::CreateHeightField(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        auto *hf = rcAllocHeightfield();
        if (hf == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate height field failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }

        if (!rcCreateHeightfield(&buildCtx, *hf, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch)) {
            buildCtx.log(RC_LOG_ERROR, "Create height field failed. %d, %d", buildParam.coord.x, buildParam.coord.y);
            return false;
        }
        rasterizeContext.solidHF = hf;
        return true;
    }
} // namespace sky::ai