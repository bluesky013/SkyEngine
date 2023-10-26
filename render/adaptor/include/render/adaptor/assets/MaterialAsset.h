//
// Created by Zach Lee on 2023/2/23.
//

#pragma once
#include <render/adaptor/assets/ImageAsset.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/resource/Material.h>
#include <unordered_map>
#include <vector>

namespace sky {
    class BinaryInputArchive;
    class BinaryOutputArchive;

    class JsonInputArchive;
    class JsonOutputArchive;

    struct MaterialTexture {
        uint32_t texIndex;
    };

    struct MaterialProperties {
        std::vector<ImageAssetPtr> images;
        std::unordered_map<std::string, Any> valueMap;
    };

    struct MaterialAssetData {
        std::vector<TechniqueAssetPtr> techniques;
        MaterialProperties defaultProperties;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;
    };

    std::shared_ptr<Material> CreateMaterial(const MaterialAssetData &data);

    template <>
    struct AssetTraits<Material> {
        using DataType                                = MaterialAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("7A82A577-959A-4735-8175-A14C26D33B6B");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<Material> CreateFromData(const DataType &data)
        {
            return CreateMaterial(data);
        }
    };
    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;

    struct MaterialInstanceData {
        MaterialAssetPtr material;
        MaterialProperties properties;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;
    };

    std::shared_ptr<MaterialInstance> CreateMaterialInstance(const MaterialInstanceData &data);

    template <>
    struct AssetTraits<MaterialInstance> {
        using DataType                                = MaterialInstanceData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("A2223B19-6566-45A4-B9BD-F12A9908BC7C");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<MaterialInstance> CreateFromData(const DataType &data)
        {
            return CreateMaterialInstance(data);
        }
    };
    using MaterialInstanceAssetPtr = std::shared_ptr<Asset<MaterialInstance>>;
} // namespace sky