//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <render/mesh/MeshRenderer.h>
#include <render/resource/SkeletonMesh.h>

namespace sky {
    class RenderScene;

    class SkeletalMeshRenderer : public MeshRenderer {
    public:
        SkeletalMeshRenderer();
        ~SkeletalMeshRenderer() override = default;

        SkinPtr GetSkin(uint32_t section) const;

        void UpdateSkinData(const Skin& skin, uint32_t section);

    protected:
        void PrepareUBO() override;
        void OnInitSubMesh(size_t subMesh) override;
        RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature, uint32_t index) override;
        void FillVertexFlags(RenderVertexFlags &flags) override;

        std::vector<RDDynamicUniformBufferPtr> boneData;
    };

} // namespace sky
