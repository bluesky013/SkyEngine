//
// Created by blues on 2024/10/8.
//

#include <recast/RecastTileGenerator.h>
#include <recast/RecastConversion.h>
#include <core/math/MathUtil.h>
#include <Recast.h>
#include <DetourTileCacheBuilder.h>
#include <DetourCommon.h>

namespace sky::ai {

    struct RecastBuildContext : public rcContext {
    public:
        RecastBuildContext() = default;
        ~RecastBuildContext() override = default;

        std::vector<RecastMeshTileData> navigationData;
    };

    RecastRasterizeContext::~RecastRasterizeContext()
    {
        rcFreeHeightField(solidHF);
        rcFreeHeightfieldLayerSet(layerSet);
        rcFreeCompactHeightfield(compactHF);
    }

    RecastTileGenerator::RecastTileGenerator(const CounterPtr<RecastNaviMesh>& mesh, const RecastTileBuildParam &param)
        : naviMesh(mesh)
        , buildParam(param)
    {
        const auto &resolution = mesh->GetResolution();
        const auto &agentCfg = mesh->GetAgentConfig();

        config.cs = resolution.cellSize;
        config.ch = resolution.cellHeight;

        config.walkableSlopeAngle = agentCfg.maxSlope;
        config.walkableHeight     = CeilTo<int>(agentCfg.height / config.ch);
        config.walkableClimb      = CeilTo<int>(agentCfg.maxClimb / config.ch);
        config.walkableRadius     = CeilTo<int>(agentCfg.radius / config.cs);

        config.tileSize   = FloorTo<int>(resolution.tileSize / config.cs);
        config.borderSize = config.walkableRadius + 3;
        config.width      = config.tileSize + config.borderSize * 2;
        config.height     = config.tileSize + config.borderSize * 2;

        // built-in params.
        config.maxEdgeLen = static_cast<int>(12.f / config.cs);
        config.maxSimplificationError = 1.3f;
        config.minRegionArea          = static_cast<int>(8.f);
        config.mergeRegionArea        = static_cast<int>(20.f);
        config.maxVertsPerPoly        = 6;
        config.detailSampleDist       = 6.f;
        config.detailSampleMaxError   = 1.f;

        const auto &bound = naviMesh->GetBounds();
        ToRecast(bound.min, config.bmin);
        ToRecast(bound.max, config.bmax);
    }

    bool RecastTileGenerator::DoWork()
    {
        return true;
    }

    void RecastTileGenerator::GenerateTile() const
    {
        RecastBuildContext context;
        if (!GenerateCompressedLayers(context)) {
            return;
        }
    }

    bool RecastTileGenerator::GenerateCompressedLayers(RecastBuildContext &context) const
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

    void RecastTileGenerator::RasterizeTriangles(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {

    }

    bool RecastTileGenerator::BuildTileCache(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        buildCtx.log(RC_LOG_PROGRESS, "Build Tile Cache.");

        auto layers = rasterizeContext.layerSet->nlayers;

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
            auto status = dtBuildTileCacheLayer(nullptr, &header, layer->heights, layer->areas, layer->cons, &outData, &outSize);
            if (dtStatusFailed(status)) {
                dtFree(outData);
                buildCtx.log(RC_LOG_ERROR, "Build tile cache layer failed..");
                return false;
            }

            // From UE version, outData allocates a lots of space
            tileData.navData = reinterpret_cast<uint8_t*>(dtAlloc(outSize * sizeof(uint8_t), DT_ALLOC_PERM));
            tileData.navDataSize = static_cast<uint32_t>(outSize);

            memcpy(tileData.navData, outData, outSize);
            dtFree(outData);
        }

        buildCtx.navigationData.swap(rasterizeContext.layerData);
        return true;
    }

    bool RecastTileGenerator::BuildLayers(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
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

    bool RecastTileGenerator::ErodeWalkableArea(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        buildCtx.log(RC_LOG_PROGRESS, "Erode Walkable Area.");

        if (!rcErodeWalkableArea(&buildCtx, config.walkableRadius, *rasterizeContext.compactHF)) {
            return false; // NOLINT
        }

        return true;
    }

    void RecastTileGenerator::FilterProcess(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
    {
        rcFilterLowHangingWalkableObstacles(&buildCtx, config.walkableClimb, *rasterizeContext.solidHF);
        rcFilterLedgeSpans(&buildCtx, config.walkableHeight, config.walkableClimb, *rasterizeContext.solidHF);
        rcFilterWalkableLowHeightSpans(&buildCtx, config.walkableHeight, *rasterizeContext.solidHF);
    }

    bool RecastTileGenerator::CreateCompactHeightField(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
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

    bool RecastTileGenerator::CreateHeightField(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const
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