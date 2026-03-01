//
// Created by Zach Lee on 2026/2/24.
//

#include "framework/asset/AssetDataBase.h"
#include "pvs/editor/PVSBakePipeline.h"

#include <../../../../engine/render/core/include/render/IStaticRenderObject.h>
#include <pvs/PVSCulling.h>
#include <pvs/editor/PVSVolume.h>
#include <pvs/editor/PVSWorldBuilder.h>
#include <render/adaptor/Util.h>

namespace sky::editor {

    PVSWorldBuilder::PVSWorldBuilder()
    {
        config.worldOffset = VEC3_ZERO;
        config.cellSize    = 800.f;
        config.cellSizeY   = 600.f;
    }

    void PVSWorldBuilder::BuildSamples(std::vector<PVSSampleParam> &samples) const
    {
        // --- Local sample positions within cell (normalized [0,1]) ---
        // Center + 8 corner offsets (inset to avoid exact boundary)
        constexpr float lo = 0.15f;
        constexpr float hi = 0.85f;

        static constexpr Vector3 localPositions[] = {
            {0.5f, 0.5f, 0.5f}, // center
            { lo,   lo,   lo },  // corners
            { hi,   lo,   lo },
            { lo,   hi,   lo },
            { hi,   hi,   lo },
            { lo,   lo,   hi },
            { hi,   lo,   hi },
            { lo,   hi,   hi },
            { hi,   hi,   hi },
        };

        // --- 6 cube face directions (axis-aligned) ---
        static constexpr Vector3 directions[] = {
            { 1.f,  0.f,  0.f}, // +X
            {-1.f,  0.f,  0.f}, // -X
            { 0.f,  1.f,  0.f}, // +Y
            { 0.f, -1.f,  0.f}, // -Y
            { 0.f,  0.f,  1.f}, // +Z
            { 0.f,  0.f, -1.f}, // -Z
        };

        // --- Combine positions × directions ---
        constexpr uint32_t NUM_POS  = static_cast<uint32_t>(std::size(localPositions));
        constexpr uint32_t NUM_DIRS = static_cast<uint32_t>(std::size(directions));
        samples.clear();
        samples.reserve(NUM_POS * NUM_DIRS);

        for (uint32_t p = 0; p < NUM_POS; ++p) {
            for (uint32_t d = 0; d < NUM_DIRS; ++d) {
                samples.push_back({localPositions[p], directions[d]});
            }
        }
    }

    void PVSWorldBuilder::Build(const WorldPtr &world) const
    {
        auto buildContext = std::make_shared<PVSBuildContext>();

        const auto& persistID = world->GetPersistID();
        const auto &workFs = AssetDataBase::Get()->GetWorkSpaceFs();
        buildContext->savePath = FilePath("Baked/PVS/" + persistID.ToString());
        workFs->MakeDir(buildContext->savePath);

        // build sample params
        BuildSamples(buildContext->samples);

        // get render scene
        auto *renderScene = GetRenderSceneFromWorld(world.Get());
        auto *pvsCulling = static_cast<PVSCulling*>(renderScene->GetCullingSystem(Name("PVS")));

        std::vector<PVSVolume*> volumes;
        std::vector<BoundingBoxSphere> bounds;
        std::vector<IStaticRenderObject*> staticObjects;

        const auto& actors = world->GetActors();
        for (const auto& actor : actors) {
            if (auto *volume = actor->GetComponent<PVSVolume>(); volume != nullptr) {
                volumes.emplace_back(volume);
            }

            StaticRenderObjectGather::BroadCast(actor.get(), &IStaticRenderObjectGather::Gather, staticObjects);
        }

        buildContext->objectGeometryInstance.resize(staticObjects.size());
        bounds.resize(staticObjects.size());

        VisibleID nextID = 0;
        for (auto& object : staticObjects) {
            object->SetObjectID(nextID);

            auto& geometryInstance = buildContext->objectGeometryInstance[nextID];
            geometryInstance.mesh = object->GetMesh();
            geometryInstance.transform = object->GetWorldTransform();
            geometryInstance.id = nextID;

            bounds[nextID] = object->GetWorldBounds();

            nextID++;
        }

        uint32_t cellDaSize = Align(nextID, 8U) / 8U;
        buildContext->cellDataSize = cellDaSize;
        buildContext->config = config;

        // build cells
        std::unordered_map<PVSCellCoord, Vector3, PVSCellCoordHash> cells;

        for (auto *volume : volumes) {
            const auto &bounds = volume->GetBounding();

            auto minCellCoord = config.CalculateCellCoordByWorldPosition(bounds.center - bounds.extent);
            auto maxCellCoord = config.CalculateCellCoordByWorldPosition(bounds.center + bounds.extent);

            for (int32_t z = minCellCoord.z; z <= maxCellCoord.z; ++z) {
                for (int32_t y = minCellCoord.y; y <= maxCellCoord.y; ++y) {
                    for (int32_t x = minCellCoord.x; x <= maxCellCoord.x; ++x) {
                        PVSCellCoord coord {x, y, z};
                        cells.emplace(coord, config.CalculateCellWorldMin(coord));
                    }
                }
            }
        }
        std::vector<Vector3> cellPositions;

        std::vector<CellBuildTask> cellBuildTask;
        buildContext->sortedCells.reserve(cells.size());
        for (const auto& pair : cells) {
            CellBuildTask task = {
                .cellID = static_cast<uint32_t>(buildContext->sortedCells.size())
            };

            for (auto& sample : buildContext->samples) {
                auto pos = pair.second + sample.localPositionInCell * Vector3(config.cellSize, config.cellSizeY, config.cellSize);
                cellPositions.emplace_back(pos);
            }

            buildContext->sortedCells.emplace_back(std::move(pair.second));
            cellBuildTask.emplace_back(task);
        }

        auto* feature = sky::GetFeatureProcessor<PVSDrawFeatureProcessor>(renderScene);
        feature->ResetBuildTask(std::move(cellBuildTask), buildContext);

        auto *visualizer = pvsCulling->GetVisualizer();
        if (visualizer != nullptr) {
            visualizer->DrawCells(config, buildContext->sortedCells);
            visualizer->DrawPrimitiveBounds(bounds);
            visualizer->DrawSamplePosition(cellPositions);
            visualizer->Flush();
        }
    }
} // namespace sky::editor} // namespace sky::editor