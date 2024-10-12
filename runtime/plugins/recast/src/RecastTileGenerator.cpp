//
// Created by blues on 2024/10/8.
//

#include <recast/RecastTileGenerator.h>
#include <recast/RecastConversion.h>
#include <recast/RecastLz4Compressor.h>

#include <Recast.h>
#include <DetourTileCacheBuilder.h>
#include <DetourCommon.h>

namespace sky::ai {

    struct RecastTileBuildContext : public rcContext {
    public:
        RecastTileBuildContext() = default;
        ~RecastTileBuildContext() override = default;

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

    bool RecastTileGenerator::RasterizeGeometry(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext)
    {
        const float* vertices = nullptr;
        int numVertices = 0;

        const int* tris = nullptr;
        int numTris = 0;

        triAreas.resize(numTris);
        rcMarkWalkableTriangles(&buildCtx, config.walkableSlopeAngle, vertices, numVertices, tris, numTris, triAreas.data());

        return rcRasterizeTriangles(&buildCtx, vertices, numVertices, tris, triAreas.data(), numTris, *rasterizeContext.solidHF, config.walkableClimb);
    }

    bool RecastTileGenerator::BuildTileCache(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        buildCtx.log(RC_LOG_PROGRESS, "Build Tile Cache.");

        auto layers = rasterizeContext.layerSet->nlayers;

        NaviCompressor compressor;

        rasterizeContext.layerData.resize(layers);
        for (int i = 0; i < layers; ++i) {
            auto &tileData = rasterizeContext.layerData[i];
            const rcHeightfieldLayer* layer = &rasterizeContext.layerSet->layers[i];

            dtTileCacheLayerHeader header;
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
            auto status = dtBuildTileCacheLayer(&compressor, &header, layer->heights, layer->areas, layer->cons, &outData, &outSize);
            if (dtStatusFailed(status)) {
                dtFree(outData);
                buildCtx.log(RC_LOG_ERROR, "Build tile cache layer failed..");
                return false;
            }

            // From UE version, outData allocates a lots of space
            auto *navData = new RecastNavData();
            navData->data = reinterpret_cast<uint8_t*>(dtAlloc(outSize * sizeof(uint8_t), DT_ALLOC_PERM));
            navData->size = static_cast<uint32_t>(outSize);
            tileData.navData = navData;

            memcpy(navData->data, outData, outSize);
            dtFree(outData);
        }

        buildCtx.navigationData.swap(rasterizeContext.layerData);
        return true;
    }

    bool RecastTileGenerator::BuildLayers(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        buildCtx.log(RC_LOG_PROGRESS, "Build Height Field Layers.");

        auto *layerSet = rcAllocHeightfieldLayerSet();
        if (layerSet == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate height field layer set failed.");
            return false;
        }

        if (!rcBuildHeightfieldLayers(&buildCtx, *rasterizeContext.compactHF, config.borderSize, config.walkableHeight, *layerSet)) {
            buildCtx.log(RC_LOG_ERROR, "Build height field layers failed.");
            return false;
        }

        return true;
    }

    bool RecastTileGenerator::ErodeWalkableArea(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        buildCtx.log(RC_LOG_PROGRESS, "Erode Walkable Area.");

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
        buildCtx.log(RC_LOG_PROGRESS, "Process Compact Height Field.");

        auto *chf = rcAllocCompactHeightfield();
        if (chf == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate compact height field failed.");
            return false;
        }

        if (!rcBuildCompactHeightfield(&buildCtx, config.walkableHeight, config.walkableClimb, *rasterizeContext.solidHF, *chf))
        {
            buildCtx.log(RC_LOG_ERROR, "Build compact data failed.");
            return false;
        }
        rasterizeContext.compactHF = chf;
        return true;
    }

    bool RecastTileGenerator::CreateHeightField(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        buildCtx.log(RC_LOG_PROGRESS, "Process Height Field.");

        auto *hf = rcAllocHeightfield();
        if (hf == nullptr) {
            buildCtx.log(RC_LOG_ERROR, "Allocate height field failed.");
            return false;
        }

        if (!rcCreateHeightfield(&buildCtx, *hf, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch)) {
            return false;
        }
        rasterizeContext.solidHF = hf;
        return true;
    }

    void RecastTileGenerator::OnComplete(bool result)
    {

    }
} // namespace sky::ai