//
// Created by blues on 2026/2/21.
//

#include <render/lod/LodMeshPrimitive.h>

#include "render/mesh/MeshLodProxy.h"

#include <render/SceneView.h>

namespace sky {

    RenderLodPrimitive::RenderLodPrimitive(const RDLodGroupPtr& inGroup) : lodGroup(inGroup)
    {
        const auto numLod = inGroup->GetLodCount();
        lodPrimitives.resize(numLod);

        for (uint32_t i = 0; i < numLod; ++i) {
            auto *lodProxy = static_cast<const MeshLodProxy*>(lodGroup->GetLod(i));
            auto &mesh = lodProxy->GetMesh();
            if (mesh) {
                lodPrimitives[i] = std::make_unique<RenderMaterialPrimitive>(mesh);
            }
        }
    }

    void RenderLodPrimitive::SetVertexFlags(const RenderVertexFlags& flags) noexcept
    {
        for (auto& prim : lodPrimitives) {
            if (prim) {
                prim->SetVertexFlags(flags);
            }
        }
    }

    void RenderLodPrimitive::SetInstanceData(const RDResourceGroupPtr& set) noexcept
    {
        for (auto& prim : lodPrimitives) {
            if (prim) {
                prim->instanceSet = set;
            }
        }
    }

    void RenderLodPrimitive::UpdateWorldBounds(const Matrix4& localToWorld) noexcept
    {
        auto &localBounds = lodGroup->GetLocalBounds();
        worldBounds = BoundingBoxSphere::Transform(localBounds, localToWorld);
    }

    void RenderLodPrimitive::Prepare(const SceneView* view) noexcept
    {
        currentLod = lodGroup->SelectLod(worldBounds.center, worldBounds.radius, view->GetViewOrigin(), view->GetProject());
    }

    bool RenderLodPrimitive::PrepareBatch(const RenderBatchPrepareInfo& info) noexcept
    {
        auto *primitive = lodPrimitives[currentLod].get();
        if (primitive != nullptr) {
            return primitive->PrepareBatch(info);
        }
        return false;
    }

    void RenderLodPrimitive::GatherRenderItem(IRenderItemGatherContext* context) noexcept
    {
        auto *primitive = lodPrimitives[currentLod].get();
        if (primitive != nullptr) {
            primitive->GatherRenderItem(context);
        }
    }

    bool RenderLodPrimitive::IsReady() const noexcept
    {
        auto *primitive = lodPrimitives[currentLod].get();
        if (primitive != nullptr) {
            return primitive->IsReady();
        }
        return true;
    }

} // namespace sky