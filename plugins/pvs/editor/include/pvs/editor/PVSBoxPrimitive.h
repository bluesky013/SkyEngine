//
// Created by Zach Lee on 2026/3/1.
//

#pragma once

#include "render/RenderScene.h"

#include <pvs/PVSVisualizer.h>
#include <render/RenderPrimitive.h>

namespace sky::editor {
    struct PVSBoxInstanceBuffer {
        Vector3 center;
        Vector3 scale;
        uint32_t id;
    };

    /**
     * @brief Per-draw instance data matching draw_id.hlsl cbuffer Local (space2).
     *
     * HLSL layout (std140):
     *   float4x4 World;  // offset 0,  size 64
     *   uint     ID;     // offset 64, size 4
     *
     * Total: 68 bytes (padded to 80 by std140 rules for cbuffer alignment).
     */
    struct PVSDrawIDLocalData {
        Matrix4  world;       // float4x4 World
        uint32_t id;          // uint ID
        uint32_t _padding[3]; // pad to 16-byte alignment (cbuffer packing)
    };

    struct PVSBoxGeometry : RenderGeometry {
        PVSBoxGeometry();
    };

    struct PVSBatchBufferData {
        Matrix4 world;
        uint32_t id;
    };

    struct PVSGeometryPrimitive : RenderPrimitive {
        explicit PVSGeometryPrimitive(const RDGfxTechPtr &tech)
            : techInst(tech)
        {
            shouldUseFrustumCulling = false;
        }

        void Update(const std::vector<PVSDrawGeometryInstance>& inInstances);
        void Reset();

        bool IsReady() const noexcept override { return !meshes.empty(); }
        bool PrepareBatch(const RenderBatchPrepareInfo& info) noexcept override;
        void GatherRenderItem(IRenderItemGatherContext* context) noexcept override;

        RenderTechniqueInstance   techInst;
        RDResourceLayoutPtr       layout;

        std::vector<RDMeshPtr> meshes;
        std::vector<RDBufferPtr> buffers;
        std::vector<RDResourceGroupPtr> resourceGroups;
        std::vector<rhi::GraphicsPipelinePtr> pipelines;
    };

} // namespace sky::editor