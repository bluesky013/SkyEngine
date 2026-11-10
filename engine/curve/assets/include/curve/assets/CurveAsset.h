#pragma once

#include <curve/Curve.h>
#include <framework/asset/Asset.h>
#include <framework/serialization/SerializationContext.h>

#include <cstdint>

namespace sky::curve {

    struct CurveAssetData {
        uint32_t version    = 1;
        uint64_t sourceHash = 0;
        Curve    curve;

        void Save(BinaryOutputArchive &archive) const;
        void Load(BinaryInputArchive &archive);
        bool IsLoadValid() const
        {
            return valid;
        }

        bool valid = false;
    };

    class CurveAsset final : public RefObject {};

    void RegisterCurveAssets(SerializationContext *context);

} // namespace sky::curve

namespace sky {
    template <>
    struct AssetTraits<curve::CurveAsset> {
        using DataType                                   = curve::CurveAssetData;
        static constexpr std::string_view ASSET_TYPE     = "Curve";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };
} // namespace sky
