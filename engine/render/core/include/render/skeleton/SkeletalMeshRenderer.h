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

        void UpdateSkinData(const SkinUpdateDataPtr& skinData);
    };

} // namespace sky
