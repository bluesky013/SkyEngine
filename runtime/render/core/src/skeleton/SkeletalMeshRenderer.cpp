//
// Created by Zach Lee on 2023/9/9.
//

#include <render/Renderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/skeleton/SkeletalMeshRenderer.h>

namespace sky {

    SkeletalMeshRenderer::SkeletalMeshRenderer()
    {
    }

    SkinPtr SkeletalMeshRenderer::GetSkin(uint32_t section) const
    {
        auto *skeletonMesh = static_cast<SkeletonMesh*>(mesh.Get());
        return skeletonMesh->GetSkin(section);
    }

    void SkeletalMeshRenderer::UpdateSkinData(const Skin& skin, uint32_t section)
    {
        if (mesh && mesh->HasSkin()) {
            boneData[section]->Write(0, reinterpret_cast<const uint8_t*>(skin.boneMatrices.data()), static_cast<uint32_t>(skin.boneMapping.size() * sizeof(Matrix4)));
            boneData[section]->Upload();
        }
    }

    void SkeletalMeshRenderer::OnInitSubMesh(size_t subMesh)
    {
        boneData.resize(subMesh);
        for (uint32_t i = 0; i < subMesh; i++) {
            boneData[i] = new DynamicUniformBuffer();
            boneData[i]->Init(MAX_BONE_NUM * sizeof(Matrix4));
        }
    }

    void SkeletalMeshRenderer::PrepareUBO()
    {
        MeshRenderer::PrepareUBO();

        if (mesh) {
            auto *skeletonMesh = static_cast<SkeletonMesh*>(mesh.Get());

            for (uint32_t i = 0; i < boneData.size(); ++i) {
                const auto &skin = skeletonMesh->GetSkin(i);
                boneData[i]->Write(0, reinterpret_cast<const uint8_t*>(skin->boneMatrices.data()), static_cast<uint32_t>(skin->boneMapping.size() * sizeof(Matrix4)));
                boneData[i]->Upload();
            }
        }
    }

    RDResourceGroupPtr SkeletalMeshRenderer::RequestResourceGroup(MeshFeature *feature, uint32_t index)
    {
        auto res = feature->RequestSkinnedResourceGroup();
        res->BindDynamicUBO(Name("skinData"), boneData[index], 0);
        return res;
    }

    void SkeletalMeshRenderer::FillVertexFlags(RenderVertexFlags &flags)
    {
        flags |= RenderVertexFlagBit::SKIN;
    }

} // namespace sky
