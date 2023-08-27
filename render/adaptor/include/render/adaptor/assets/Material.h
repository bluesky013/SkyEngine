//
// Created by Zach Lee on 2023/2/23.
//

#pragma once
#include <render/adaptor/assets/Technique.h>
#include <render/adaptor/assets/Image.h>
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

    struct MaterialAssetData {
        std::vector<TechniqueAssetPtr> techniques;
        std::vector<ImageAssetPtr> images;
        std::unordered_map<std::string, Any> valueMap;

        void LoadBin(BinaryInputArchive &archive);
        void SaveBin(BinaryOutputArchive &archive) const;

        void LoadJson(JsonInputArchive &archive);
        void SaveJson(JsonOutputArchive &archive) const;
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
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;
    };
    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;
}