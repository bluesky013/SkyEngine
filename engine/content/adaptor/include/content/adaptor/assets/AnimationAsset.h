//
// Animation assets for the content adaptor: versioned CPU payloads of an
// AnimationClip (bone channels) and an animation graph. Bridges the
// framework-free `Animation` module to the framework asset system.
//

#pragma once

#include <animation/core/Animation.h>
#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationNodeChannel.h>
#include <core/util/Uuid.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sky {

    struct AnimationClipAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t                         version = CURRENT_VERSION;
        std::string                      name;
        float                            frameRate = 0.f;
        Uuid                             skeleton; // bound skeleton asset (optional)
        std::vector<AnimNodeChannelData> nodeChannels;

        void Load(BinaryInputArchive &ar);
        void Save(BinaryOutputArchive &ar) const;
        void clear();
    };

    using AnimationClipAssetPtr = std::shared_ptr<Asset<AnimationClip>>;

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset);

    struct AnimationAssetData {
        static constexpr uint32_t CURRENT_VERSION = 1;

        uint32_t version = CURRENT_VERSION;

        void Load(BinaryInputArchive &ar);
        void Save(BinaryOutputArchive &ar) const;
        void clear();
    };

} // namespace sky

namespace sky {

    template <>
    struct AssetTraits<AnimationClip> {
        using DataType                                   = AnimationClipAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AnimationClip";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

    template <>
    struct AssetTraits<Animation> {
        using DataType                                   = AnimationAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AnimationGraph";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
