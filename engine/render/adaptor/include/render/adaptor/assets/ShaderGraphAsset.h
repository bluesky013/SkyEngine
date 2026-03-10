//
// Created by blues on 2026/3/10.
//

#pragma once

#include <framework/asset/AssetManager.h>
#include <shader/shadergraph/ShaderGraph.h>

namespace sky {
    class JsonInputArchive;
    class JsonOutputArchive;

    struct ShaderGraphAssetData {
        uint32_t   version = 1;
        sg::ShaderGraph graph;

        void LoadJson(JsonInputArchive& archive);
        void SaveJson(JsonOutputArchive& archive) const;
    };

    // Placeholder type tag for the shader graph asset
    struct ShaderGraphAssetTag {};

    template <>
    struct AssetTraits<ShaderGraphAssetTag> {
        using DataType                                = ShaderGraphAssetData;
        static constexpr std::string_view ASSET_TYPE  = "ShaderGraph";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;
    };

} // namespace sky
