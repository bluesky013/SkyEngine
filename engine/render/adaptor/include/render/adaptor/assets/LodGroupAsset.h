//
// Created by blues on 2026/2/17.
//

#pragma once

#include <animation/core/Skeleton.h>
#include <framework/asset/AssetManager.h>
#include <framework/interface/IMeshConfigNotify.h>
#include <render/lod/LodGroup.h>

namespace sky {

    struct LodGroupLevelData {
        float screenSize = 1.f;
        Uuid resId;
    };

    struct LodGroupData {
        uint32_t version = 0;
        std::string type;
        std::vector<LodGroupLevelData> levels;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<LodGroup> {
        using DataType                                = LodGroupData;
        static constexpr std::string_view ASSET_TYPE  = "LodGroup";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using LodGroupAssetPtr = std::shared_ptr<Asset<LodGroup>>;

    struct LodGroupBuildData {
        CounterPtr<LodGroup> group;
        SkeletonPtr skeleton;
    };

    LodGroupBuildData CreateLodGroupFromAsset(const LodGroupAssetPtr &asset, bool buildSkin = false);

} // namespace sky
