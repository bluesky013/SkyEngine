//
// Created by blues on 2024/8/2.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <animation/core/Animation.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationClip.h>
#include <animation/graph/AnimationState.h>
#include <string>
#include <vector>

namespace sky {


    struct AnimationClipAssetData {
        uint32_t version;
        std::string name;
        float frameRate;
        Uuid skeleton;
        std::vector<AnimNodeChannelData> nodeChannels;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<AnimationClip> {
        using DataType                                = AnimationClipAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Clip";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using AnimationClipAssetPtr = std::shared_ptr<Asset<AnimationClip>>;

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset);

    struct AnimationAssetData {
        uint32_t version;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Animation> {
        using DataType                                = AnimationAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Graph";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using AnimationAssetPtr = std::shared_ptr<Asset<Animation>>;

    struct AnimationStateClipData {
        std::string name;
        Uuid clip;
    };

    struct AnimationStateConditionData {
        std::string parameter; // from animation
    };

    struct AnimationStateData {
        std::vector<AnimationStateClipData> nodes;
        std::vector<AnimationStateConditionData> conditions;
        std::vector<AnimTransition> transitions;
    };
} // namespace sky