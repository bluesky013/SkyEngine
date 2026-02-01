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

        SkinPtr GetSkin() const;

        void UpdateSkinData(const Skin& skin);

    protected:
        void PrepareUBO() override;
        RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature) override;
        void FillVertexFlags(RenderVertexFlags &flags) override;

        RDDynamicUniformBufferPtr boneData;
    };

} // namespace sky
