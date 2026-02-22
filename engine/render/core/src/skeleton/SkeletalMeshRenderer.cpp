//
// Created by Zach Lee on 2023/9/9.
//

#include <render/Renderer.h>
#include <render/mesh/MeshFeature.h>
#include <render/skeleton/SkeletalMeshRenderer.h>

namespace sky {

    // void SkeletalMeshRenderer::UpdateSkinData(const SkinPtr& skin, uint32_t section)
    // {
        // if (mesh && mesh->HasSkin()) {
        //     boneData[section]->Write(0, reinterpret_cast<const uint8_t*>(skin.boneMatrices.data()), static_cast<uint32_t>(skin.boneMapping.size() * sizeof(Matrix4)));
        //     boneData[section]->Upload();
        // }
    // }

    void SkeletalMeshRenderer::UpdateSkinData(const SkinUpdateDataPtr& skinData)
    {
        lodPrimitive->UpdateSkinData(skinData);
    }


} // namespace sky
