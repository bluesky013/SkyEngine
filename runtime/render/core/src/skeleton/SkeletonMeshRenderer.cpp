//
// Created by Zach Lee on 2023/9/9.
//

#include <render/skeleton/SkeletonMeshRenderer.h>
#include <render/Renderer.h>
#include <render/mesh/MeshFeature.h>

namespace sky {

    SkeletonMeshRenderer::SkeletonMeshRenderer()
    {
        boneData = new DynamicUniformBuffer();
        boneData->Init(MAX_BONE_NUM * sizeof(Matrix4));
    }


    void SkeletonMeshRenderer::PrepareUBO()
    {
        MeshRenderer::PrepareUBO();

        if (mesh && mesh->HasSkin()) {
            auto *skeletonMesh = static_cast<SkeletonMesh*>(mesh.Get());
            const auto &skin = skeletonMesh->GetSkin();
            boneData->Write(0, reinterpret_cast<const uint8_t*>(skin->boneMatrices.data()), skin->activeBone * sizeof(Matrix4));
            boneData->Upload();
        }
    }

    RDResourceGroupPtr SkeletonMeshRenderer::RequestResourceGroup(MeshFeature *feature)
    {
        auto res = feature->RequestSkinnedResourceGroup();
        res->BindDynamicUBO(Name("skinData"), boneData, 0);
        return res;
    }

    void SkeletonMeshRenderer::FillVertexFlags(RenderVertexFlags &flags)
    {
        flags &= RenderVertexFlagBit::SKIN;
    }

} // namespace sky
