//
// Created by blues on 2024/8/10.
//

#include <render/adaptor/assets/SkeletonAsset.h>

namespace sky {

    void SkeletonAssetBuildContext::AdddBone(const Name &name, const Matrix4 &matrix)
    {
        SKY_ASSERT(!nameToIndexMap.contains(name))

        auto boneIndex = static_cast<uint32_t>(nameToIndexMap.size());
        nameToIndexMap.emplace(name, boneIndex);
        data.boneData.emplace_back(BoneData{name, INVALID_BONE_ID});
        data.inverseBindMatrix.emplace_back(matrix);
        data.refPos.emplace_back(Transform::GetIdentity());
    }

    uint32_t SkeletonAssetBuildContext::FindBoneByName(const Name &name) const
    {
        auto iter = nameToIndexMap.find(name);
        return iter != nameToIndexMap.end() ? iter->second : INVALID_BONE_ID;
    }

    void SkeletonAssetData::LoadJson(JsonInputArchive &archive)
    {
        uint32_t count = archive.StartArray("bones");

        boneData.resize(count);
        inverseBindMatrix.resize(count);
        refPos.resize(count);

        for (uint32_t i = 0; i < count; ++i) {

            archive.Start("name");
            std::string name;
            boneData[i].name = Name(archive.LoadString().c_str());
            archive.End();

            archive.Start("parent");
            boneData[i].parentIndex = archive.LoadUint();
            archive.End();

            archive.Start("inverseBindMatrix");
            archive.LoadValueObject(inverseBindMatrix[i]);
            archive.End();

            archive.Start("refPos");
            archive.LoadValueObject(refPos[i]);
            archive.End();

            archive.NextArrayElement();
        }
        archive.End();
    }

    void SkeletonAssetData::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();
        archive.Key("bones");
        archive.StartArray();

        size_t boneNum = boneData.size();
        for (size_t i = 0; i < boneNum; ++i) {
            archive.StartObject();

            archive.Key("name");
            archive.SaveValue(boneData[i].name.GetStr());

            archive.Key("parent");
            archive.SaveValue(boneData[i].parentIndex);

            archive.Key("inverseBindMatrix");
            archive.SaveValueObject(inverseBindMatrix[i]);

            archive.Key("refPos");
            archive.SaveValueObject(refPos[i]);

            archive.EndObject();
        }
        archive.EndArray();
        archive.EndObject();
    }

    void SkeletonAssetData::LoadBin(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        uint32_t count = 0;

        archive.LoadValue(count);
        boneData.resize(count);
        inverseBindMatrix.resize(count);
        refPos.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            archive.LoadValue(boneData[i].name);
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
    }

    void SkeletonAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);

        archive.SaveValue(static_cast<uint32_t>(boneData.size()));
        for (const auto &bone : boneData) {
            archive.SaveValue(bone.name);
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
    }

} // namespace sky::adaptor
