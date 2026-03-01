//
// Created by Zach Lee on 2026/2/24.
//

#include <pvs/PVSVisualizer.h>

namespace sky {

    PVSVisualizer::PVSVisualizer(RenderScene* scene, const RDTechniquePtr &tech)
        : volumeRender(std::make_unique<VolumeRenderer>(scene, tech))
    {
    }

    void PVSVisualizer::DrawPrimitiveBounds(const std::vector<BoundingBoxSphere> &instances)
    {
        for (auto& instance : instances) {
            AABB aabb;
            aabb.min = instance.center - instance.extent;
            aabb.max = instance.center + instance.extent;
            volumeRender->Draw(aabb, Color(0, 0, 1, 0.5f));
        }
    }

    void PVSVisualizer::DrawSamplePosition(const std::vector<Vector3> &instances)
    {
        for (auto& instance : instances) {
            AABB aabb;
            aabb.min = instance;
            aabb.max = instance + Vector3(50, 50, 50);
            volumeRender->Draw(aabb, Color(0, 1, 1, 0.5f));
        }
    }

    void PVSVisualizer::DrawCells(const PVSConfig &config, const std::vector<Vector3> &cells)
    {
        for (const auto &min : cells) {
            AABB aabb;
            aabb.min = min;
            aabb.max = min + Vector3(config.cellSize, config.cellSizeY, config.cellSize);
            volumeRender->Draw(aabb, Color(1, 0, 0, 0.5f));
        }
    }

    void PVSVisualizer::Flush()
    {
        volumeRender->Update();
    }

} // namespace sky