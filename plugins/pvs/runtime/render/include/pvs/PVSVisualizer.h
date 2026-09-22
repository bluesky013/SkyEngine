//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <render/debug/VolumeRenderer.h>
#include <render/resource/Mesh.h>
#include <pvs/PVSLoader.h>

namespace sky {
    struct PVSDrawGeometryInstance {
        RDMeshPtr mesh;
        Matrix4 transform;
        uint32_t id;
    };

    class PVSVisualizer {
    public:
        PVSVisualizer(RenderScene* scene, const RDTechniquePtr &tech);
        ~PVSVisualizer() = default;

        void DrawPrimitiveBounds(const std::vector<BoundingBoxSphere> &instances);

        void DrawSamplePosition(const std::vector<Vector3> &instances);

        void DrawCells(const PVSConfig& config, const std::vector<Vector3> &cells);

        void Flush();

    private:
        std::unique_ptr<VolumeRenderer> volumeRender;
    };

} // namespace sky