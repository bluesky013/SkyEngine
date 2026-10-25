// The resident clip header indexes time/key arrays in the product bulk.

#pragma once

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
        static constexpr uint32_t CURRENT_VERSION       = 3;
        static constexpr uint32_t LEGACY_SOURCE_VERSION = 2;

        uint32_t                       version = CURRENT_VERSION;
        std::string                    name;
        float                          frameRate        = 0.f;
        uint32_t                       sourceFrameCount = 0;
        Uuid                           skeleton; // bound skeleton asset (optional)
        std::vector<AnimChannelHeader> channels;

        // Build-side scratch (not serialized): the decoded/compressed channels.
        std::vector<AnimNodeChannelData> nodeChannels;
        AnimationStatus                  validation;
        bool                             IsLoadValid() const
        {
            return static_cast<bool>(validation);
        }

        // Product header (channel table; times/keys live in the bulk).
        void Load(BinaryInputArchive &ar);
        void Save(BinaryOutputArchive &ar) const;

        // Authoring source (.clip): inline channel arrays (backward compatible).
        AnimationStatus LoadSource(BinaryInputArchive &ar);
        AnimationStatus SaveSource(BinaryOutputArchive &ar);
        AnimationStatus PrepareSource();

        // Lay out the (post-compress) nodeChannels into `channels` and return the
        // bulk bytes (position -> scale -> rotation, times then keys per track).
        std::vector<uint8_t> BuildBulk();

        void clear();
    };

    using AnimationClipAssetPtr = std::shared_ptr<Asset<AnimationClip>>;

    AnimClipPtr CreateAnimationClipFromAsset(const AnimationClipAssetPtr &asset, AnimationStatus *status = nullptr);

} // namespace sky

namespace sky {

    template <>
    struct AssetTraits<AnimationClip> {
        using DataType                                   = AnimationClipAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AnimationClip";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
