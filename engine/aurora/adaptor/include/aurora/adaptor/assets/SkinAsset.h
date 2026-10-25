#pragma once

#include <aurora/resource/Skin.h>
#include <framework/asset/Asset.h>

namespace sky {
    class Skeleton;
}

namespace sky::aurora {

    struct SkinAssetData {
        static constexpr uint32_t CURRENT_VERSION = 2;
        uint32_t                  version         = CURRENT_VERSION;
        Uuid                      sourceOwner;
        SkinData                  binding;
        SkinStatus                validation;

        void Load(BinaryInputArchive &ar);
        void Save(BinaryOutputArchive &ar) const;
        bool IsLoadValid() const
        {
            return static_cast<bool>(validation);
        }
        void clear();
    };

} // namespace sky::aurora

namespace sky {
    template <>
    struct AssetTraits<aurora::Skin> {
        using DataType                                   = aurora::SkinAssetData;
        static constexpr std::string_view ASSET_TYPE     = "AuroraSkin";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };
} // namespace sky

namespace sky::aurora {
    CounterPtr<Skin> FetchOrCreateSkinByAsset(const std::shared_ptr<Asset<Skin>> &asset, SkinStatus *status = nullptr);
    SkinStatus       MakeSkinTargetLayout(const Skeleton &skeleton, SkinTargetLayout &out);
} // namespace sky::aurora
