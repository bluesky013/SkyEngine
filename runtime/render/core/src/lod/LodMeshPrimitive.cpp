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

        auto numMaxSection = lodGroup->GetMaxSectionNumWithSkin();
        boneData.resize(numMaxSection);
        for (uint32_t i = 0; i < numMaxSection; ++i) {
            boneData[i] = new DynamicUniformBuffer();
            boneData[i]->Init(MAX_BONE_NUM * sizeof(Matrix4));
        }

        for (uint32_t i = 0; i < numLod; ++i) {
            auto *lodProxy = static_cast<const MeshLodProxy*>(lodGroup->GetLod(i));
            auto &mesh = lodProxy->GetMesh();
            if (mesh) {
                lodPrimitives[i] = std::make_unique<RenderMaterialPrimitive>(mesh);
                if (mesh->HasSkin()) {
                    lodPrimitives[i]->SetBoneData(boneData);
                    lodPrimitives[i]->SetVertexFlags(RenderVertexFlagBit::SKIN);
                }
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
        auto *proxy = lodGroup->GetLod(currentLod);
        if (proxy != nullptr) {
            const auto &localBounds = proxy->GetLocalBounds();
            worldBounds = BoundingBoxSphere::Transform(localBounds, localToWorld);
        }
    }

    void RenderLodPrimitive::UpdateSkinData(const SkinUpdateDataPtr skinData) noexcept
    {
        auto *primitive = lodPrimitives[currentLod].get();
        auto *proxy = lodGroup->GetLod(currentLod);

        if (primitive == nullptr || proxy == nullptr || !proxy->HasSkin()) {
            return;
        }

        const auto* meshProxy = static_cast<const SkeletalMeshLodProxy*>(proxy);
        const auto& mesh = meshProxy->GetMesh();

        for (uint32_t i = 0; i < primitive->sections.size(); ++i) {
            const auto& bindSkin = mesh->GetSkin(i);
            if (!bindSkin) {
                continue;
            }

            const auto& bindMatrix = bindSkin->boneMatrices;

            std::vector<Matrix4> boneMatrices(MAX_BONE_NUM);
            for (uint32_t index = 0; index <bindSkin->boneMapping.size(); ++index) {
                boneMatrices[index] = skinData->boneMatrices[bindSkin->boneMapping[index]] * bindMatrix[index];
            }

            boneData[i]->Write(0, reinterpret_cast<const uint8_t*>(boneMatrices.data()), static_cast<uint32_t>(boneMatrices.size() * sizeof(Matrix4)));
            boneData[i]->Upload();
        }
    }

    void RenderLodPrimitive::Prepare(const SceneView* view) noexcept
    {
        currentLod = lodGroup->SelectLod(worldBounds.center, worldBounds.radius, view->GetViewOrigin(), view->GetProject());

        // TODO
        const auto *primitive = lodPrimitives[currentLod].get();
        if (primitive != nullptr) {
            for (auto& mat : primitive->materials) {
                mat->UploadTextures();
            }
        }

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