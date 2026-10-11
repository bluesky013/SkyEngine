//
// Animation assets for the content adaptor: versioned CPU payloads of an
// AnimationClip (bone channels) and an animation graph. Bridges the
// framework-free `Animation` module to the framework asset system.
//
// The clip header (resident) carries the channel table (per-track offset/size);
// the times/keys bytes live in the product bulk and are read on demand by
// CreateAnimationClipFromAsset.
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

    // Byte range of one time/key array relative to the clip's product bulk.
    struct AnimTrackHeader {
        uint64_t offset = 0;
        uint64_t size   = 0;
    };

    struct AnimChannelHeader {
        std::string     name;
        AnimTrackHeader positionTimes;
        AnimTrackHeader positionKeys;
        AnimTrackHeader scaleTimes;
        AnimTrackHeader scaleKeys;
        AnimTrackHeader rotationTimes;
        AnimTrackHeader rotationKeys;
    };

    struct AnimationClipAssetData {
        static constexpr uint32_t CURRENT_VERSION = 2;

        uint32_t                       version = CURRENT_VERSION;
        std::string                    name;
        float                          frameRate = 0.f;
        Uuid                           skeleton; // bound skeleton asset (optional)
        std::vector<AnimChannelHeader> channels;

        // Build-side scratch (not serialized): the decoded/compressed channels.
        std::vector<AnimNodeChannelData> nodeChannels;

        // Product header (channel table; times/keys live in the bulk).
        void Load(BinaryInputArchive &ar);
        void Save(BinaryOutputArchive &ar) const;

        // Authoring source (.clip): inline channel arrays (backward compatible).
        void LoadSource(BinaryInputArchive &ar);

        // Lay out the (post-compress) nodeChannels into `channels` and return the
        // bulk bytes (position -> scale -> rotation, times then keys per track).
        std::vector<uint8_t> BuildBulk();

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
