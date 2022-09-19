//
// Created by Zach Lee on 2022/5/7.
//

#pragma once

#include <render/resources/Buffer.h>
#include <render/resources/DescirptorGroup.h>
#include <render/resources/RenderResource.h>
#include <render/resources/Technique.h>
#include <render/resources/Texture.h>
#include <core/type/Any.h>

namespace sky {

    struct MaterialType {
        std::vector<GfxTechniqueAssetPtr> gfxTechniques;
        std::unordered_map<Uuid, std::string> assetPathMap;

        template <class Archive>
        void save(Archive &ar) const
        {
            std::vector<Uuid> uuids;
            uuids.reserve(gfxTechniques.size());
            for (auto &tech : gfxTechniques) {
                uuids.emplace_back(tech->GetUuid());
            }
            ar(uuids, assetPathMap);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            std::vector<Uuid> uuids;
            ar(uuids, assetPathMap);
            InitTechnique(uuids);
        }

        void InitTechnique(const std::vector<Uuid> &gfxIds);
    };
    using MaterialTypeAssetPtr = std::shared_ptr<Asset<MaterialType>>;

    enum class MaterialPropertyType : uint32_t {
        FLOAT,
        INT,
        UINT,
        INT64,
        UINT64,
        DOUBLE,
        VEC2,
        VEC3,
        VEC4,
        BOOL,
        TEXTURE
    };

    struct PropertyAssetData {
        std::string name;
        MaterialPropertyType type;
        Any any;

        template <class Archive>
        void save(Archive &ar) const
        {
            ar(name, type);
            if (type == MaterialPropertyType::FLOAT) {
                ar(*(any.GetAsConst<float>()));
            } else if (type == MaterialPropertyType::INT) {
                ar(*(any.GetAsConst<int32_t>()));
            } else if (type == MaterialPropertyType::UINT) {
                ar(*(any.GetAsConst<uint32_t>()));
            } else if (type == MaterialPropertyType::INT64) {
                ar(*(any.GetAsConst<int64_t>()));
            } else if (type == MaterialPropertyType::UINT64) {
                ar(*(any.GetAsConst<uint64_t>()));
            } else if (type == MaterialPropertyType::DOUBLE) {
                ar(*(any.GetAsConst<double>()));
            } else if (type == MaterialPropertyType::VEC2) {
                ar(*(any.GetAsConst<Vector2>()));
            } else if (type == MaterialPropertyType::VEC3) {
                ar(*(any.GetAsConst<Vector3>()));
            } else if (type == MaterialPropertyType::VEC4) {
                ar(*(any.GetAsConst<Vector4>()));
            } else if (type == MaterialPropertyType::BOOL) {
                ar(*(any.GetAsConst<bool>()));
            } else if (type == MaterialPropertyType::TEXTURE) {
                auto id = any.GetAsConst<Asset<Image>*>();
                ar((*id)->GetUuid());
            }
        }

        template <typename T, class Archive>
        void MakeAny(Archive &ar)
        {
            T value;
            ar(value);
            any = Any(value);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            ar(name, type);
            if (type == MaterialPropertyType::FLOAT) {
                MakeAny<float, Archive>(ar);
            } else if (type == MaterialPropertyType::INT) {
                MakeAny<int32_t, Archive>(ar);
            } else if (type == MaterialPropertyType::UINT) {
                MakeAny<uint32_t, Archive>(ar);
            } else if (type == MaterialPropertyType::INT64) {
                MakeAny<int64_t , Archive>(ar);
            } else if (type == MaterialPropertyType::UINT64) {
                MakeAny<uint64_t , Archive>(ar);
            } else if (type == MaterialPropertyType::DOUBLE) {
                MakeAny<double, Archive>(ar);
            } else if (type == MaterialPropertyType::VEC2) {
                MakeAny<Vector2, Archive>(ar);
            } else if (type == MaterialPropertyType::VEC3) {
                MakeAny<Vector3, Archive>(ar);
            } else if (type == MaterialPropertyType::VEC4) {
                MakeAny<Vector4, Archive>(ar);
            } else if (type == MaterialPropertyType::BOOL) {
                MakeAny<bool, Archive>(ar);
            } else if (type == MaterialPropertyType::TEXTURE) {
                MakeAny<Uuid, Archive>(ar);
            }
        }
    };

    struct MaterialAssetData {
        std::vector<PropertyAssetData> properties;
        MaterialTypeAssetPtr materialType;
        std::unordered_map<Uuid, std::string> assetPathMap;

        template <class Archive>
        void save(Archive &ar) const
        {
            ar(properties, materialType->GetUuid(), assetPathMap);
        }

        template <class Archive>
        void load(Archive &ar)
        {
            Uuid id;
            ar(properties, id, assetPathMap);
            InitMaterialType(id);
        }

        void InitMaterialType(const Uuid &id);
    };

    class Material : public RenderResource {
    public:
        Material()           = default;
        ~Material() override = default;

        void AddGfxTechnique(const RDGfxTechniquePtr &tech);

        const std::vector<RDGfxTechniquePtr> &GetGraphicTechniques() const;

        void InitRHI();

        RDDesGroupPtr GetMaterialSet() const;

        template <typename T>
        void UpdateValue(const std::string &name, const T &value)
        {
            auto table = matSet->GetProperTable();
            auto iter  = table->handleMap.find(name);
            if (iter == table->handleMap.end()) {
                return;
            }
            bufferViews[iter->second.binding]->Write(value, iter->second.offset);
        }

        void UpdateTexture(const std::string &name, const RDTexturePtr &tex)
        {
            auto table = matSet->GetProperTable();
            auto iter  = table->handleMap.find(name);
            if (iter == table->handleMap.end()) {
                return;
            }
            textures[iter->second.binding] = tex;
        }

        void Update();

        static std::shared_ptr<Material> CreateFromData(const MaterialAssetData &data);

    private:
        std::vector<RDGfxTechniquePtr>                gfxTechniques;
        RDDesGroupPtr                                 matSet;
        RDBufferPtr                                   materialBuffer;
        std::unordered_map<uint32_t, RDBufferViewPtr> bufferViews;
        std::unordered_map<uint32_t, RDTexturePtr>    textures;
    };

    using RDMaterialPtr = std::shared_ptr<Material>;

    template <>
    struct AssetTraits<Material> {
        using DataType                                = MaterialAssetData;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("7A82A577-959A-4735-8175-A14C26D33B6B");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static RDMaterialPtr CreateFromData(const DataType &data)
        {
            return Material::CreateFromData(data);
        }
    };
    using MaterialAssetPtr = std::shared_ptr<Asset<Material>>;

    template <>
    struct AssetTraits<MaterialType> {
        using DataType                                = MaterialType;
        static constexpr Uuid          ASSET_TYPE     = Uuid::CreateFromString("05CEFCD1-9D0F-4AE3-9232-9349F76562FF");
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static std::shared_ptr<MaterialType> CreateFromData(const DataType &data)
        {
            return {};
        }
    };
} // namespace sky