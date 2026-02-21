//
// Created by blues on 2026/2/21.
//

#pragma once

#include "render/resource/SkeletonMesh.h"

#include <render/mesh/MeshPrimitive.h>

namespace sky {

    struct RenderLodPrimitive : RenderPrimitive {
        explicit RenderLodPrimitive(const RDLodGroupPtr& inGroup);

        void Prepare(const SceneView* view) noexcept override;
        bool PrepareBatch(const RenderBatchPrepareInfo& info) noexcept override;
        void GatherRenderItem(IRenderItemGatherContext* context) noexcept override;
        bool IsReady() const noexcept override;
        void UpdateWorldBounds(const Matrix4& localToWorld) noexcept override;

        void UpdateSkinData(const SkinUpdateDataPtr skinData) noexcept;
        void SetVertexFlags(const RenderVertexFlags& flags) noexcept;
        void SetInstanceData(const RDResourceGroupPtr& set) noexcept;

        uint32_t currentLod = 0;
        RDLodGroupPtr lodGroup;
        std::vector<RDDynamicUniformBufferPtr> boneData;
        std::vector<std::unique_ptr<RenderMaterialPrimitive>> lodPrimitives;
    };

} // namespace sky
