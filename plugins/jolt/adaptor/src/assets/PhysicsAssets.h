#pragma once

#include "PhysicsAssetCodec.h"
#include <framework/asset/Asset.h>

namespace sky::phy {
    struct CollisionAsset : RefObject {};
    struct MaterialAsset : RefObject {};
    void RegisterPhysicsAssets();
} // namespace sky::phy

namespace sky {
    template <>
    struct AssetTraits<phy::CollisionAsset> {
        using DataType                               = phy::CollisionSource;
        static constexpr std::string_view ASSET_TYPE = phy::COLLISION_ASSET_TYPE;
    };
    template <>
    struct AssetTraits<phy::MaterialAsset> {
        using DataType                               = phy::PhysicsMaterialData;
        static constexpr std::string_view ASSET_TYPE = phy::MATERIAL_ASSET_TYPE;
    };
} // namespace sky
