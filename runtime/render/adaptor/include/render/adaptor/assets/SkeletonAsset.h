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

        uint32_t AdddBone(const std::string &name, const Matrix4 &matrix);
        uint32_t FindBoneByName(const std::string &name) const;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Skeleton> {
        using DataType                                = SkeletonAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Skeleton";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using SkeletonAssetPtr = std::shared_ptr<Asset<Skeleton>>;
} // namespace sky