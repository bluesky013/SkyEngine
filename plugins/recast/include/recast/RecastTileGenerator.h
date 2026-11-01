//
// Created by blues on 2024/10/8.
//

#pragma once

#include <Recast.h>
#include <core/async/Task.h>
#include <recast/RecastNaviMesh.h>

struct rcHeightfield;
struct rcHeightfieldLayerSet;
struct rcCompactHeightfield;

struct dtTileCacheAlloc;
struct dtTileCacheLayer;
struct dtTileCacheContourSet;
struct dtTileCachePolyMesh;
struct dtTileCacheCompressor;

namespace sky::ai {
    struct RecastTileBuildContext;

    struct RecastRasterizeContext {
        RecastRasterizeContext() = default;
        ~RecastRasterizeContext();

        rcHeightfield                  *solidHF   = nullptr;
        rcHeightfieldLayerSet          *layerSet  = nullptr;
        rcCompactHeightfield           *compactHF = nullptr;
        std::vector<RecastMeshTileData> layerData;
    };

    struct RecastTileCacheContext {
        RecastTileCacheContext() = default;
        ~RecastTileCacheContext();

        dtTileCacheAlloc      *allocator  = nullptr;
        dtTileCacheCompressor *compressor = nullptr;

        dtTileCacheLayer               *cacheLayer = nullptr;
        dtTileCacheContourSet          *contourSet = nullptr;
        dtTileCachePolyMesh            *polyMesh   = nullptr;
        std::vector<RecastMeshTileData> layerData;
    };

    struct RecastTileBuildParam {
        RecastTile coord;
    };

    class RecastTileGenerator : public Task {
    public:
        explicit RecastTileGenerator(const rcConfig &cfg, const RecastTileBuildParam &param);
        ~RecastTileGenerator() override = default;

        void Setup(const std::shared_ptr<NaviOctree> &octree);

        std::vector<RecastMeshTileData> &GetData()
        {
            return data;
        }

        const RecastTileBuildParam &GetParam() const
        {
            return buildParam;
        }
        bool HasSucceeded() const
        {
            return succeeded.load();
        }

    private:
        bool DoWork() override;

        bool GenerateTile();
        bool GenerateCompressedLayers(RecastTileBuildContext &context);
        bool CreateHeightField(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool CreateCompactHeightField(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        void FilterProcess(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool ErodeWalkableArea(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool BuildLayers(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool BuildTileCache(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext);

        bool RasterizeTriangles(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext);
        bool RasterizeGeometry(RecastTileBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext);

        rcConfig                    config;
        RecastTileBuildParam        buildParam;
        std::shared_ptr<NaviOctree> octree;
        NaviCompressor              compressor;
        std::atomic_bool            succeeded{false};

        std::vector<uint8_t>            triAreas;
        std::vector<RecastMeshTileData> data;
    };

} // namespace sky::ai