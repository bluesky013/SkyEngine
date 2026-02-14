//
// Created by blues on 2024/9/29.
//

#pragma once

#include <render/resource/Mesh.h>
#include <render/resource/StaticMesh.h>

namespace sky {

    static constexpr uint32_t MAX_BONE_PER_VERTEX = 4;
    static constexpr uint32_t MAX_BONE_NUM = 256;

    struct Skin : RefObject {
        std::array<Matrix4, MAX_BONE_NUM> boneMatrices;
        std::vector<uint8_t> boneMapping;
    };
    using SkinPtr = CounterPtr<Skin>;

    struct VertexBoneData {
        uint8_t boneId[MAX_BONE_PER_VERTEX];
        float weight[MAX_BONE_NUM];
    };

    class SkeletalMeshGeometry : public StaticMeshGeometry {
    public:
        SkeletalMeshGeometry() = default;
        ~SkeletalMeshGeometry() override = default;

        void SetBoneIndexAndWeight(uint32_t vertexIndex, const VertexBoneData& data);

        MeshVertexDataInterface* GetBoneAndWeight() const { return boneWeightBuffer.get(); }

    private:
        void OnInit(uint32_t vertexNum, uint32_t indexNum, rhi::IndexType idxType, const Config& config) override;

        std::unique_ptr<MeshVertexDataInterface> boneWeightBuffer;
    };

    class SkeletonMesh : public Mesh {
    public:
        SkeletonMesh() = default;
        ~SkeletonMesh() override = default;

        void SetSkin(const SkinPtr &skin_, uint32_t index)
        {
            if (index >= skins.size()) {
                skins.resize(index + 1);
            }

            skins[index] = skin_;
        }

        const SkinPtr &GetSkin(uint32_t index) const
        {
            return skins[index];
        }

        bool HasSkin() const override { return true; }
    private:
        std::vector<SkinPtr> skins;
    };
    using RDSkeletonMeshPtr = CounterPtr<Mesh>;
} // namespace sky
