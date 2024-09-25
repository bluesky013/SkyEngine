//
// Created by Zach Lee on 2023/9/9.
//

#include <render/skeleton/SkeletonMeshRenderer.h>
#include <render/Renderer.h>
#include <render/mesh/MeshFeature.h>

namespace sky {

    SkeletonMeshRenderer::SkeletonMeshRenderer()
    {
        skin = new Skin();
        for (uint32_t i = 0; i < MAX_BONE_NUM; ++i) {
            skin->boneMatrices[i] = Matrix4::Identity();
        }
    }

    void SkeletonMeshRenderer::PrepareUBO()
    {
        MeshRenderer::PrepareUBO();

        boneData = new DynamicUniformBuffer();
        boneData->Init(sizeof(Skin));
        boneData->WriteT(0, skin->boneMatrices.data());
        boneData->Upload();
    }

    RDResourceGroupPtr SkeletonMeshRenderer::RequestResourceGroup(MeshFeature *feature)
    {
        return feature->RequestSkinnedResourceGroup();
    }

} // namespace sky
