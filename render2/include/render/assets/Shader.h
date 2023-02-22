//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct ShaderAssetData {
        std::vector<uint32_t> data;

        void Load(BinaryInputArchive &archive);
        void Save(BinaryOutputArchive &archive) const;
    };

    class Shader {
    public:
        Shader() = default;
        ~Shader() = default;
    };

    template <>
    struct AssetTraits<Shader> {
        using DataType                                = ShaderAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("E71838F5-40F3-470A-883C-401D8796B5FD");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using ShaderAssetPtr = std::shared_ptr<Asset<Shader>>;
}