//
// Created by blues on 2024/8/10.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <animation/skeleton/Skeleton.h>
#include <string>
#include <vector>

namespace sky {

    struct SkeletonAssetData : public SkeletonData {
        uint32_t version = 0;
        
        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;
    };

    struct SkeletonAssetBuildContext {
        void AdddBone(const Name &name, const Matrix4 &matrix);
        uint32_t FindBoneByName(const Name &name) const;
        
        std::unordered_map<Name, uint32_t> nameToIndexMap;
        SkeletonAssetData data;
    };

    template <>
    struct AssetTraits<Skeleton> {
        using DataType                                = SkeletonAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Skeleton";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using SkeletonAssetPtr = std::shared_ptr<Asset<Skeleton>>;
} // namespace sky
