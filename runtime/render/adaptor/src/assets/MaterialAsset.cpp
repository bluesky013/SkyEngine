//
// Created by Zach Lee on 2023/2/23.
//

#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <core/template/Overloaded.h>

namespace sky {
    void LoadProperties(BinaryInputArchive &archive, MaterialProperties &properties)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        properties.images.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(properties.images[i]);
        }

        size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            MaterialValueType type;
            archive.LoadValue(type);
            switch (type) {
                case MaterialValueType::TEXTURE: {
                    MaterialTexture tex = {};
                    archive.LoadValue(tex.texIndex);
                    properties.valueMap[key] = tex;
                }
                break;
                case MaterialValueType::VEC2: {
                    Vector2 vec = {};
                    archive.LoadValue(reinterpret_cast<char *>(&vec), sizeof(Vector2));
                    properties.valueMap[key] = vec;
                }
                break;
                case MaterialValueType::VEC3: {
                    Vector3 vec = {};
                    archive.LoadValue(reinterpret_cast<char *>(&vec), sizeof(Vector3));
                    properties.valueMap[key] = vec;
                }
                break;
                case MaterialValueType::VEC4: {
                    Vector4 vec = {};
                    archive.LoadValue(reinterpret_cast<char *>(&vec), sizeof(Vector4));
                    properties.valueMap[key] = vec;
                }
                break;
                case MaterialValueType::FLOAT: {
                    float val = {};
                    archive.LoadValue(val);
                    properties.valueMap[key] = val;
                }
                break;
                case MaterialValueType::U32: {
                    uint32_t val = {};
                    archive.LoadValue(val);
                    properties.valueMap[key] = val;
                }
                break;
                case MaterialValueType::I32: {
                    int32_t val = {};
                    archive.LoadValue(val);
                    properties.valueMap[key] = val;
                }
                break;
                default:
                    SKY_UNEXPECTED;
            }
        }
    }

    void SaveProperties(BinaryOutputArchive &archive, const MaterialProperties &properties) {
        archive.SaveValue(static_cast<uint32_t>(properties.images.size()));
        for (const auto &image: properties.images) {
            archive.SaveValue(image);
        }

        archive.SaveValue(static_cast<uint32_t>(properties.valueMap.size()));
        for (const auto &[key, value]: properties.valueMap) {
            archive.SaveValue(key);

            std::visit(Overloaded{
                    [&archive](const MaterialTexture &v) {
                        archive.SaveValue(MaterialValueType::TEXTURE);
                        archive.SaveValue(v.texIndex);
                    },
                    [&archive](const Vector2 &v) {
                        archive.SaveValue(MaterialValueType::VEC2);
                        archive.SaveValue(reinterpret_cast<const char *>(&v), sizeof(Vector2));
                    },
                    [&archive](const Vector3 &v) {
                        archive.SaveValue(MaterialValueType::VEC3);
                        archive.SaveValue(reinterpret_cast<const char *>(&v), sizeof(Vector3));
                    },
                    [&archive](const Vector4 &v) {
                        archive.SaveValue(MaterialValueType::VEC4);
                        archive.SaveValue(reinterpret_cast<const char *>(&v), sizeof(Vector4));
                    },
                    [&archive](const float &v) {
                        archive.SaveValue(MaterialValueType::FLOAT);
                        archive.SaveValue(v);
                    },
                    [&archive](const uint32_t &v) {
                        archive.SaveValue(MaterialValueType::U32);
                        archive.SaveValue(v);
                    },
                    [&archive](const int32_t &v) {
                        archive.SaveValue(MaterialValueType::I32);
                        archive.SaveValue(v);
                    },
            }, value);
        }
    }

    void MaterialAssetData::LoadBin(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        techniques.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(techniques[i]);
        }

        LoadProperties(archive, defaultProperties);
    }

    void MaterialAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(techniques.size()));
        for (const auto &tech :techniques) {
            archive.SaveValue(tech);
        }
        SaveProperties(archive, defaultProperties);
    }

    void MaterialInstanceData::LoadBin(BinaryInputArchive &archive)
    {
        auto *am = AssetManager::Get();
        archive.LoadValue(material);
        LoadProperties(archive, properties);
    }

    void MaterialInstanceData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(material);
        SaveProperties(archive, properties);
    }

    std::shared_ptr<Material> CreateMaterial(const MaterialAssetData &data)
    {
        auto *am = AssetManager::Get();

        auto mat = std::make_shared<Material>();
        for (const auto &tech : data.techniques) {
            mat->AddTechnique(am->LoadAsset<Technique>(tech)->CreateInstanceAs<GraphicsTechnique>());
        }
        return mat;
    }

    std::shared_ptr<MaterialInstance> CreateMaterialInstance(const MaterialInstanceData &data)
    {
        auto *am = AssetManager::Get();

        auto mi = std::make_shared<MaterialInstance>();
        mi->SetMaterial(am->LoadAsset<Material>(data.material)->CreateInstance());
        for (const auto &[key, val] : data.properties.valueMap) {
            std::visit(Overloaded{
                    [&mi, &data, &am, key_ = key](const MaterialTexture &v) {
                        auto imageAsset = am->LoadAsset<Texture>(data.properties.images[v.texIndex]);
                        mi->SetTexture(key_, imageAsset->CreateInstance(), 0);
                    },
                    [&mi, key_ = key](const auto &v) {
                        mi->SetValue(key_, reinterpret_cast<const uint8_t*>(&v), sizeof(decltype(v)));
                    }
            }, val);
        }
        mi->Upload();
        return mi;
    }
} // namespace sky