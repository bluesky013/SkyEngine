//
// Created by blues on 2024/8/2.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <animation/core/Animation.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationClip.h>
#include <string>
#include <vector>

namespace sky {


    struct AnimationAssetData {
        uint32_t version;
        std::string name;
        std::vector<AnimNodeChannelData> nodeChannels;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Animation> {
        using DataType                                = AnimationAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Animation";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using AnimationAssetPtr = std::shared_ptr<Asset<Animation>>;

    AnimClipPtr CreateAnimationFromAsset(const AnimationAssetPtr &asset);
} // namespace sky