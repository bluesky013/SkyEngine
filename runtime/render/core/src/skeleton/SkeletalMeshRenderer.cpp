//
// Created by Zach Lee on 2023/9/9.
//

#include <render/Renderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/skeleton/SkeletalMeshRenderer.h>

namespace sky {

    SkinPtr SkeletalMeshRenderer::GetSkin(uint32_t section) const
    {
        // auto *skeletonMesh = static_cast<SkeletonMesh*>(mesh.Get());
        // return skeletonMesh->GetSkin(section);
        return nullptr;
    }

    void SkeletalMeshRenderer::UpdateSkinData(const Skin& skin, uint32_t section)
    {
        // if (mesh && mesh->HasSkin()) {
        //     boneData[section]->Write(0, reinterpret_cast<const uint8_t*>(skin.boneMatrices.data()), static_cast<uint32_t>(skin.boneMapping.size() * sizeof(Matrix4)));
        //     boneData[section]->Upload();
        // }
    }

    void SkeletalMeshRenderer::OnInitSubMesh(size_t subMesh)
    {
        boneData.resize(subMesh);
        for (uint32_t i = 0; i < subMesh; i++) {
            boneData[i] = new DynamicUniformBuffer();
            boneData[i]->Init(MAX_BONE_NUM * sizeof(Matrix4));
        }
    }

    void SkeletalMeshRenderer::Init()
    {
        InitUBO();
        auto resourceGroup = MeshFeature::Get()->RequestSkinnedResourceGroup();
        // res->BindDynamicUBO(Name("skinData"), boneData[index], 0);
    }

} // namespace sky
