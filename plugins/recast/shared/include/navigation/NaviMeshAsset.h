//
// Created on 2026/10/30.
//

#pragma once

#include <framework/asset/Asset.h>
#include <navigation/NaviMeshTypes.h>

namespace sky::ai {
    using NaviMeshAssetPtr = std::shared_ptr<Asset<NaviMesh>>;
    CounterPtr<NaviMesh> CreateNaviMeshFromAsset(const NaviMeshAssetPtr &asset);
} // namespace sky::ai

namespace sky {

    template <>
    struct AssetTraits<ai::NaviMesh> {
        using DataType                                   = ai::NaviMeshData;
        static constexpr std::string_view ASSET_TYPE     = "NaviMesh";
        static constexpr SerializeType    SERIALIZE_TYPE = SerializeType::BIN;
    };

} // namespace sky
