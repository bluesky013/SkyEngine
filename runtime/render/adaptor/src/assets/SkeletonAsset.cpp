//
// Created by blues on 2024/8/10.
//

#include <render/adaptor/assets/SkeletonAsset.h>

namespace sky {

    std::string ReplaceBoneNamespace(const std::string& boneName, const std::string& newNamespace)
    {
        size_t colonPos = boneName.find(':');
        if (colonPos == std::string::npos) {
            return boneName;
        }
        return newNamespace + boneName.substr(colonPos);
    }

    void SkeletonAssetBuildContext::AdddBone(const std::string &name, const Matrix4 &matrix)
    {
        SKY_ASSERT(!nameToIndexMap.contains(name))

        auto boneIndex = static_cast<BoneIndex>(nameToIndexMap.size());
        nameToIndexMap.emplace(name, boneIndex);
        data.boneData.emplace_back(BoneData{Name{}, INVALID_BONE_ID});
        data.refPos.emplace_back(Transform::GetIdentity());

        inverseBindMatrix.emplace_back(matrix);
    }

    BoneIndex SkeletonAssetBuildContext::FindBoneByName(const std::string &name) const
    {
        auto iter = nameToIndexMap.find(name);
        return iter != nameToIndexMap.end() ? iter->second : INVALID_BONE_ID;
    }

    void SkeletonAssetBuildContext::FillBoneName(const std::string &ns)
    {
        for (auto& [name, index] : nameToIndexMap) {
            std::string boneName = ns.empty() ? name : ReplaceBoneNamespace(name, ns);
            data.boneData[index].name = Name(boneName.c_str());
        }
    }

    void SkeletonAssetData::LoadJson(JsonInputArchive &archive)
    {
        uint32_t count = archive.StartArray("bones");

        boneData.resize(count);
        refPos.resize(count);

        for (uint32_t i = 0; i < count; ++i) {

            archive.Start("name");
            std::string name;
            boneData[i].name = Name(archive.LoadString().c_str());
            archive.End();

            archive.Start("parent");
            boneData[i].parentIndex = archive.LoadUint();
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
        refPos.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            std::string name;
            archive.LoadValue(name);

            boneData[i].name = Name(name.c_str());
            archive.LoadValue(boneData[i].parentIndex);
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
            archive.SaveValue(bone.name.GetStr());
            archive.SaveValue(bone.parentIndex);
        }

        for (const auto &trans : refPos) {
            archive.SaveValue(reinterpret_cast<const char*>(trans.translation.v), sizeof(Vector3));
            archive.SaveValue(reinterpret_cast<const char*>(trans.rotation.v), sizeof(Quaternion));
            archive.SaveValue(reinterpret_cast<const char*>(trans.scale.v), sizeof(Vector3));
        }
    }

} // namespace sky::adaptor
