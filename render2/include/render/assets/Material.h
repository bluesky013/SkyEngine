//
// Created by Zach Lee on 2023/2/23.
//

#pragma once
#include <render/assets/Technique.h>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    struct MaterialAssetData {
        std::vector<TechniqueAssetPtr> techniques;

        void Load(BinaryInputArchive &archive) {}
        void Save(BinaryOutputArchive &archive) const {}
    };

    class Material {
    public:
        Material() = default;
        ~Material() = default;
    };

    template <>
    struct AssetTraits<Material> {
        using DataType                                = MaterialAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("7A82A577-959A-4735-8175-A14C26D33B6B");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;
}