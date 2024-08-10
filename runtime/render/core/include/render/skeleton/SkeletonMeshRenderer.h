//
// Created by Zach Lee on 2023/9/9.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Matrix4.h>
#include <render/mesh/MeshRenderer.h>
#include <vector>

namespace sky {
    class RenderScene;

    static constexpr uint32_t MAX_BONE_PER_VERTEX = 4;
    static constexpr uint32_t MAX_BONE_NUM = 80;

    struct Skin : RefObject {
        std::array<Matrix4, MAX_BONE_NUM> boneMatrices;
    };
    using SkinPtr = CounterPtr<Skin>;

    struct VertexBoneData {
        uint32_t boneId[MAX_BONE_PER_VERTEX];
        float weight[MAX_BONE_PER_VERTEX];
    };

    class SkeletonMeshRenderer : public MeshRenderer {
    public:
        SkeletonMeshRenderer();
        ~SkeletonMeshRenderer() override = default;

        SkinPtr GetSkin() const { return skin; }

        void SetBoneTransform(uint32_t index, const Matrix4 &trans);
    protected:
        void PrepareUBO() override;
        RDResourceGroupPtr RequestResourceGroup(MeshFeature *feature) override;

        SkinPtr skin;
        RDDynamicUniformBufferPtr boneData;
    };

} // namespace sky
