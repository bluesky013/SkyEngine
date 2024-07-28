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
        uint32_t version;
        std::vector<Uuid> techniques;
        MaterialProperties defaultProperties;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<Material> {
        using DataType                                = MaterialAssetData;
        static constexpr std::string_view ASSET_TYPE  = "Material";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;

    struct MaterialInstanceData {
        uint32_t version;
        Uuid material;
        MaterialProperties properties;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
    };

    template <>
    struct AssetTraits<MaterialInstance> {
        using DataType                                = MaterialInstanceData;
        static constexpr std::string_view ASSET_TYPE  = "MaterialInstance";
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::BIN;
    };
    using MaterialInstanceAssetPtr = std::shared_ptr<Asset<MaterialInstance>>;

    CounterPtr<Material> CreateMaterialFromAsset(const MaterialAssetPtr &asset);
    CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(const MaterialInstanceAssetPtr &asset);
} // namespace sky