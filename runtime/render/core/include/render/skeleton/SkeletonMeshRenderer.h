//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Matrix4.h>
#include <render/mesh/MeshRenderer.h>
#include <render/resource/SkeletonMesh.h>
#include <vector>

namespace sky {
    class RenderScene;

    class SkeletonMeshRenderer : public MeshRenderer {
    public:
        SkeletonMeshRenderer();
        ~SkeletonMeshRenderer() override = default;

    protected:
        void PrepareUBO() override;
        RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature) override;
        void FillVertexFlags(RenderVertexFlags &flags) override;
        RDDynamicUniformBufferPtr boneData;
    };

} // namespace sky
