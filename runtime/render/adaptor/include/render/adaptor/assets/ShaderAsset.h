//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/Asset.h>
#include <render/resource/Shader.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct ShaderAssetData {
        std::string shaderSource;
        std::string name;
        uint32_t hash;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<ShaderCollection> {
        using DataType                                = ShaderAssetData;
        static constexpr std::string_view ASSET_TYPE  = "ShaderCollection";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using ShaderAssetPtr = std::shared_ptr<Asset<ShaderCollection>>;

    CounterPtr<ShaderCollection> CreateShaderFromAsset(const ShaderAssetPtr &asset);
}
