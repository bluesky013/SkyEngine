//
// Created on 2026/09/21.
//

#pragma once

#include <audio/AudioClip.h>
#include <framework/asset/Asset.h>
#include <framework/asset/AssetCommon.h>

#include <memory>

namespace sky {

    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct AudioClipData {
        AudioClipDesc desc;
        AssetRawData  rawData;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    using AudioClipAssetPtr = std::shared_ptr<Asset<AudioClip>>;

    template <>
    struct AssetTraits<AudioClip> {
        using DataType                                = AudioClipData;
        static constexpr std::string_view ASSET_TYPE  = "AudioClip";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };

    CounterPtr<AudioClip> CreateAudioClipFromAsset(const AudioClipAssetPtr &asset);

} // namespace sky
