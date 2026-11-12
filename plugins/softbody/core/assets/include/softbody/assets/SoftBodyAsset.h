#pragma once

#include <framework/asset/Asset.h>
#include <physics/softbody/SoftBodyTypes.h>

#include <span>

namespace sky::softbody {
    inline constexpr std::string_view SOFT_BODY_ASSET_TYPE        = "SoftBody";
    inline constexpr uint32_t         MAX_SOFT_BODY_PARTICLES     = 255;
    inline constexpr size_t           MAX_SOFT_BODY_PRODUCT_BYTES = 16u * 1024u * 1024u;

    enum class SoftBodyNormalPolicy : uint32_t { TetGradient = 0, StandardSkinning, END };

    struct SoftBodyAsset : RefObject {};
    struct SoftBodyAssetData {
        uint32_t          version = 2;
        Uuid              mesh;
        Uuid              skin;
        Uuid              skeleton;
        uint64_t          skeletonLayout = 0;
        phy::SoftBodyDesc body;
        // Particle i drives skeleton bone i + 1; bone zero is the identity root.
        std::vector<std::string> controlIds;
        SoftBodyNormalPolicy     normalPolicy = SoftBodyNormalPolicy::TetGradient;
    };

    phy::PhysicsStatus ValidateSoftBodyAsset(const SoftBodyAssetData &data);
    phy::PhysicsStatus EncodeSoftBodyAsset(const SoftBodyAssetData &data, std::vector<uint8_t> &out);
    phy::PhysicsStatus DecodeSoftBodyAsset(std::span<const uint8_t> bytes, SoftBodyAssetData &out);
    void               RegisterSoftBodyAssets();
} // namespace sky::softbody

namespace sky {
    template <>
    struct AssetTraits<softbody::SoftBodyAsset> {
        using DataType                                   = softbody::SoftBodyAssetData;
        static constexpr std::string_view ASSET_TYPE     = softbody::SOFT_BODY_ASSET_TYPE;
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };
} // namespace sky
