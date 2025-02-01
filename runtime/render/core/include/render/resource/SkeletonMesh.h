//
// Created by blues on 2024/9/29.
//

#pragma once

#include <render/resource/Mesh.h>

namespace sky {

    static constexpr uint32_t MAX_BONE_PER_VERTEX = 4;
    static constexpr uint32_t MAX_BONE_NUM = 80;

    struct Skin : RefObject {
        std::array<Matrix4, MAX_BONE_NUM> boneMatrices;
        uint32_t activeBone = 0;
    };
    using SkinPtr = CounterPtr<Skin>;

    struct VertexBoneData {
        uint32_t boneId[MAX_BONE_PER_VERTEX];
        float weight[MAX_BONE_PER_VERTEX];
    };

    class SkeletonMesh : public Mesh {
    public:
        SkeletonMesh() = default;
        ~SkeletonMesh() override = default;

        void SetSkin(const SkinPtr &skin_) { skin = skin_; }
        const SkinPtr &GetSkin() const { return skin; }

        bool HasSkin() const override { return static_cast<bool>(skin); }

    private:
        SkinPtr skin;
    };
    using RDSkeletonMeshPtr = CounterPtr<Mesh>;
} // namespace sky
