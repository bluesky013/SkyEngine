//
// Created by Zach Lee on 2026/2/24.
//

#pragma once

#include <Render/RenderPrimitive.h>

namespace sky {
    class RenderScene;

    struct BoxGeometryInstanceData {
        Vector3 center;
        Vector3 extent;
        Color color;
    };

    struct VolumeBoxLineGeometry : RenderGeometry {
        VolumeBoxLineGeometry();
    };

    struct VolumePrimitive : RenderPrimitive {
        explicit VolumePrimitive(const RDGfxTechPtr& inTech)
            : techInst(inTech)
        {
            shouldUseFrustumCulling = false;
            geometry = new VolumeBoxLineGeometry();
            techInst.SetVertexFlags(RenderVertexFlagBit::INSTANCE);
        }

        void Update(std::vector<BoxGeometryInstanceData>&& data) noexcept;

        bool IsReady() const noexcept override { return geometry && (!!geometry->vertexBuffers[1].buffer); }
        bool PrepareBatch(const RenderBatchPrepareInfo& info) noexcept override;
        void GatherRenderItem(IRenderItemGatherContext* context) noexcept override;

        rhi::GraphicsPipelinePtr  pso;
        rhi::VertexInputPtr       vertexDesc;
        RenderTechniqueInstance   techInst;

        RDDynamicBuffer instanceBuffer;
        DrawArgs args;
    };

    class VolumeRenderer {
    public:
        VolumeRenderer(RenderScene* scene, const RDTechniquePtr &tech);
        ~VolumeRenderer() noexcept;

        void Draw(const AABB& bounding, const Color& color = Color(0, 1, 0, 1)) noexcept;
        void Draw(const BoundingBox& bounding, const Color& color = Color(0, 1, 0, 1)) noexcept;
        void Draw(const BoundingBoxSphere& bounding, const Color& color = Color(0, 1, 0, 1)) noexcept;
        void Update() noexcept;

    private:
        void ResetPrimitive() noexcept;

        RenderScene* scene;
        std::unique_ptr<VolumePrimitive> primitive;

        std::vector<BoxGeometryInstanceData> data;
    };

} // namespace sky

