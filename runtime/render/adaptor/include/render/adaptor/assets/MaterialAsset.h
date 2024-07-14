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

    struct MaterialProperties {
        std::vector<Uuid> images;
        std::unordered_map<std::string, MaterialValue> valueMap;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
    };

    struct MaterialAssetData {
        std::vector<Uuid> techniques;
        MaterialProperties defaultProperties;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;
    };

    std::shared_ptr<Material> CreateMaterial(const MaterialAssetData &data);

    template <>
    struct AssetTraits<Material> {
        using DataType                                = MaterialAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Material";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<Material> CreateFromData(const DataType &data)
        {
            return CreateMaterial(data);
        }
    };
    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;

    struct MaterialInstanceData {
        Uuid material;
        MaterialProperties properties;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
    };

    std::shared_ptr<MaterialInstance> CreateMaterialInstance(const MaterialInstanceData &data);

    template <>
    struct AssetTraits<MaterialInstance> {
        using DataType                                = MaterialInstanceData;
        static constexpr std::string_view ASSET_TYPE  = "MaterialInstance";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;

        static std::shared_ptr<MaterialInstance> CreateFromData(const DataType &data)
        {
            return CreateMaterialInstance(data);
        }
    };
    using MaterialInstanceAssetPtr = std::shared_ptr<Asset<MaterialInstance>>;
} // namespace sky