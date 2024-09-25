//
// Created by blues on 2024/8/10.
//

#include <render/adaptor/assets/SkeletonAsset.h>

namespace sky {

    uint32_t SkeletonAssetData::AdddBone(const std::string &name, const Matrix4 &matrix)
    {
        SKY_ASSERT(!nameToIndexMap.contains(name))

        auto boneIndex = static_cast<uint32_t>(nameToIndexMap.size());
        nameToIndexMap.emplace(name, boneIndex);
        boneData.emplace_back(name, INVALID_BONE_ID);
        inverseBindMatrix.emplace_back(matrix);
        refPos.emplace_back(Transform::GetIdentity());
        return boneIndex;
    }

    uint32_t SkeletonAssetData::FindBoneByName(const Name &name) const
    {
        auto iter = nameToIndexMap.find(name);
        return iter != nameToIndexMap.end() ? iter->second : INVALID_BONE_ID;
    }

    void SkeletonAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        uint32_t count = 0;

        archive.LoadValue(count);
        boneData.resize(count);
        inverseBindMatrix.resize(count);
        refPos.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            archive.LoadValue(boneData[i].parentIndex);

        }

        for (uint32_t i = 0; i < count; ++i) {
            archive.LoadValue(reinterpret_cast<char*>(inverseBindMatrix[i].v), sizeof(Matrix4));
        }

        for (uint32_t i = 0; i < count; ++i) {
            archive.LoadValue(reinterpret_cast<char*>(refPos[i].translation.v), sizeof(Vector3));
            archive.LoadValue(reinterpret_cast<char*>(refPos[i].rotation.v), sizeof(Quaternion));
            archive.LoadValue(reinterpret_cast<char*>(refPos[i].scale.v), sizeof(Vector3));
        }

        archive.LoadValue(count);
        for (uint32_t i = 0; i < count; ++i) {
            std::string name;
            archive.LoadValue(name);
            uint32_t boneIndex = 0;
            archive.LoadValue(boneIndex);

            nameToIndexMap.emplace(name, boneIndex);
        }
    }

    void SkeletonAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);

        archive.SaveValue(static_cast<uint32_t>(boneData.size()));
        for (const auto &bone : boneData) {
            archive.SaveValue(bone.parentIndex);

        }

        for (const auto &mtx : inverseBindMatrix) {
            archive.SaveValue(reinterpret_cast<const char*>(mtx.v), sizeof(Matrix4));
        }

        for (const auto &trans : refPos) {
            archive.SaveValue(reinterpret_cast<const char*>(trans.translation.v), sizeof(Vector3));
            archive.SaveValue(reinterpret_cast<const char*>(trans.rotation.v), sizeof(Quaternion));
            archive.SaveValue(reinterpret_cast<const char*>(trans.scale.v), sizeof(Vector3));
        }

        archive.SaveValue(static_cast<uint32_t>(nameToIndexMap.size()));
        for (const auto &[name, index] : nameToIndexMap) {
            archive.SaveValue(name);
            archive.SaveValue(index);
        }
    }

} // namespace sky::adaptor