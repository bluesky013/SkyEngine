//
// Created by blues on 2024/10/8.
//

#pragma once

#include <core/async/Task.h>
#include <recast/RecastNaviMesh.h>
#include <Recast.h>

struct rcHeightfield;
struct rcHeightfieldLayerSet;
struct rcCompactHeightfield;

namespace sky::ai {
    struct RecastBuildContext;

    struct RecastRasterizeContext {
        RecastRasterizeContext() = default;
        ~RecastRasterizeContext();

        rcHeightfield*         solidHF   = nullptr;
        rcHeightfieldLayerSet* layerSet  = nullptr;
        rcCompactHeightfield*  compactHF = nullptr;
        std::vector<RecastMeshTileData> layerData;
    };

    struct RecastTileBuildParam {
        RecastTile coord;
    };

    class RecastTileGenerator : public Task {
    public:
        explicit RecastTileGenerator(const CounterPtr<RecastNaviMesh>& mesh, const RecastTileBuildParam &param);
        ~RecastTileGenerator() override = default;

    private:
        bool DoWork() override;
        void OnComplete(bool result) override;

        void GenerateTile() const;
        bool GenerateCompressedLayers(RecastBuildContext &context) const;
        bool CreateHeightField(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        void RasterizeTriangles(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool CreateCompactHeightField(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        void FilterProcess(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool ErodeWalkableArea(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool BuildLayers(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;
        bool BuildTileCache(RecastBuildContext &buildCtx, RecastRasterizeContext &rasterizeContext) const;


        CounterPtr<RecastNaviMesh> naviMesh;
        RecastTileBuildParam buildParam;

        RecastMeshTileData data;
        rcConfig config;
    };

} // namespace sky::ai