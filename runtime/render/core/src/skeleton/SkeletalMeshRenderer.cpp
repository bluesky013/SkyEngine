//
// Created by Zach Lee on 2023/9/9.
//

#include <render/Renderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/skeleton/SkeletalMeshRenderer.h>

namespace sky {

    SkeletalMeshRenderer::SkeletalMeshRenderer()
    {
        boneData = new DynamicUniformBuffer();
        boneData->Init(MAX_BONE_NUM * sizeof(Matrix4));
    }

    SkinPtr SkeletalMeshRenderer::GetSkin() const
    {
        auto *skeletonMesh = static_cast<SkeletonMesh*>(mesh.Get());
        return skeletonMesh->GetSkin();
    }

    void SkeletalMeshRenderer::UpdateSkinData(const Skin& skin)
    {
        if (mesh && mesh->HasSkin()) {
            boneData->Write(0, reinterpret_cast<const uint8_t*>(skin.boneMatrices.data()), skin.activeBone * sizeof(Matrix4));
            boneData->Upload();
        }
    }

    void SkeletalMeshRenderer::PrepareUBO()
    {
        MeshRenderer::PrepareUBO();

        if (mesh && mesh->HasSkin()) {
            auto *skeletonMesh = static_cast<SkeletonMesh*>(mesh.Get());
            const auto &skin = skeletonMesh->GetSkin();
            boneData->Write(0, reinterpret_cast<const uint8_t*>(skin->boneMatrices.data()), skin->activeBone * sizeof(Matrix4));
            boneData->Upload();
        }
    }

    RDResourceGroupPtr SkeletalMeshRenderer::RequestResourceGroup(MeshFeature *feature)
    {
        auto res = feature->RequestSkinnedResourceGroup();
        res->BindDynamicUBO(Name("skinData"), boneData, 0);
        return res;
    }

    void SkeletalMeshRenderer::FillVertexFlags(RenderVertexFlags &flags)
    {
        flags |= RenderVertexFlagBit::SKIN;
    }

} // namespace sky
