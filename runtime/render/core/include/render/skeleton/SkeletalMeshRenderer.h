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
        explicit SkeletalMeshRenderer(RenderScene* inScene) : MeshRenderer(inScene) {}
        ~SkeletalMeshRenderer() override = default;

        SkinPtr GetSkin(uint32_t section) const;

        void UpdateSkinData(const Skin& skin, uint32_t section);

    protected:
        void Init() override;
        void OnInitSubMesh(size_t subMesh) override;

        std::vector<RDDynamicUniformBufferPtr> boneData;
    };

} // namespace sky
