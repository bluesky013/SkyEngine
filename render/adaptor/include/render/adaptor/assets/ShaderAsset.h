//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/Asset.h>
#include <render/resource/Shader.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct ShaderVariantData {
        std::string gles;
        std::vector<uint32_t> spv;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    class ShaderVariant {
    public:
        ShaderVariant() = default;
        ~ShaderVariant() = default;
    };

    template <>
    struct AssetTraits<ShaderVariant> {
        using DataType                                = ShaderVariantData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("AEFF7E05-0211-4585-9381-1DF3AC5E5E78");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using ShaderVariantAssetPtr = std::shared_ptr<Asset<ShaderVariant>>;


    struct ShaderAssetData {
        std::string source;
        std::unordered_map<std::string, ShaderVariantAssetPtr> variants;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Shader> {
        using DataType                                = ShaderAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("E71838F5-40F3-470A-883C-401D8796B5FD");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using ShaderAssetPtr = std::shared_ptr<Asset<Shader>>;
}