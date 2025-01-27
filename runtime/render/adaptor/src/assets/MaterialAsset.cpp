//
// Created by Zach Lee on 2023/2/23.
//

#include <framework/serialization/SerializationContext.h>
#include <framework/asset/AssetDataBase.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <core/template/Overloaded.h>
#include <core/profile/Profiler.h>

namespace sky {
    void LoadProperties(BinaryInputArchive &archive, MaterialProperties &properties)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            MaterialValueType type;
            archive.LoadValue(type);
            switch (type) {
                case MaterialValueType::TEXTURE: {
                    MaterialTexture tex = {};
                    archive.LoadValue(reinterpret_cast<char *>(&tex.texID), sizeof(Uuid));
                    properties.valueMap[key] = tex;
                }
                break;
                case MaterialValueType::SAMPLER: {
                    TextureSampler tex = {};
                    archive.LoadValue(reinterpret_cast<char *>(&tex), sizeof(TextureSampler));
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
        archive.SaveValue(static_cast<uint32_t>(properties.valueMap.size()));
        for (const auto &[key, value]: properties.valueMap) {
            archive.SaveValue(key);

            std::visit(Overloaded{
                    [&archive](const MaterialTexture &v) {
                        archive.SaveValue(MaterialValueType::TEXTURE);
                        archive.SaveValue(reinterpret_cast<const char *>(&v.texID), sizeof(Uuid));
                    },
                    [&archive](const TextureSampler &v) {
                        archive.SaveValue(MaterialValueType::SAMPLER);
                        archive.SaveValue(reinterpret_cast<const char *>(&v), sizeof(TextureSampler));
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
                    }
            }, value);
        }
    }

    void MaterialAssetData::LoadBin(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        uint32_t size = 0;
        archive.LoadValue(size);

        for (uint32_t i = 0; i < size; ++i) {
            Uuid uuid;
            archive.LoadValue(uuid);
            techniques.emplace(uuid);
        }

        LoadProperties(archive, defaultProperties);
    }

    void MaterialAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(static_cast<uint32_t>(techniques.size()));
        for (const auto &tech :techniques) {
            archive.SaveValue(tech);
        }
        SaveProperties(archive, defaultProperties);
    }

    void MaterialInstanceData::LoadBin(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);
        auto *am = AssetManager::Get();
        archive.LoadValue(material);
        LoadProperties(archive, properties);
    }

    void MaterialInstanceData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(material);
        SaveProperties(archive, properties);
    }

    void MaterialInstanceData::LoadJson(JsonInputArchive &archive)
    {
        archive.Start("material");
        material = Uuid::CreateFromString(archive.LoadString());
        archive.End();

        properties.LoadJson(archive);
    }

    void MaterialInstanceData::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();

        archive.Key("material");

        auto source = AssetDataBase::Get()->FindAsset(material);
        if (source) {
            archive.SaveValue(source->path.path.GetStr());
        } else {
            archive.SaveValue("");
        }

        properties.SaveJson(archive);

        archive.EndObject();
    }

    void MaterialProperties::LoadJson(JsonInputArchive &archive)
    {
        archive.Start("properties");

        archive.ForEachMember([this](const std::string &key, const auto &obj) {
            if (obj.IsString()) {
                auto path = obj.GetString();
                auto asset = AssetDataBase::Get()->FindAsset(path);
                valueMap[key] = MaterialTexture{asset->uuid};
            } else if (obj.IsFloat()) {
                valueMap[key] = obj.GetFloat();
            } else if (obj.IsBool()) {
                valueMap[key] = static_cast<uint32_t>(obj.GetBool());
            } else if (obj.IsUint()) {
                valueMap[key] = static_cast<uint32_t>(obj.GetUint());
            } else if (obj.IsArray()) {
                auto   array = obj.GetArray();
                float *v     = nullptr;
                if (array.Size() == 2) {
                    v = std::get<Vector2>(valueMap.emplace(key, Vector2{}).first->second).v;
                } else if (array.Size() == 3) {
                    v = std::get<Vector3>(valueMap.emplace(key, Vector3{}).first->second).v;
                } else if (array.Size() == 4) {
                    v = std::get<Vector4>(valueMap.emplace(key, Vector4{}).first->second).v;
                }
                for (auto &val : array) {
                    (*v) = val.GetFloat();
                    ++v;
                }
            }
        });
        archive.End();
    }

    void MaterialProperties::SaveJson(JsonOutputArchive &archive) const
    {
        archive.Key("properties");
        archive.StartObject();
        for (const auto &[key, value] : valueMap) {
            archive.Key(key.c_str());
            std::visit(Overloaded{
                    [&archive, this](const MaterialTexture &tex) {
                        auto asset = AssetDataBase::Get()->FindAsset(tex.texID);
                        if (asset) {
                            archive.SaveValue(asset->path.path.GetStr());
                        } else {
                            archive.SaveValue("");
                        }
                    },
                    [&archive](const auto& arg) { archive.SaveValueObject(arg); }
            }, value);
        }
        archive.EndObject();
    }

    CounterPtr<Material> CreateMaterialFromAsset(const MaterialAssetPtr &asset)
    {
        auto &data = asset->Data();

        auto *am = AssetManager::Get();
        auto *mat = new Material();
        for (const auto &tech : data.techniques) {
            auto techAsset = am->LoadAsset<Technique>(tech);
            mat->AddTechnique(GreateGfxTechFromAsset(techAsset));
        }

        MaterialPropertyInitializer initializer(data.defaultProperties.valueMap.size());
        for (auto &[key, val] : data.defaultProperties.valueMap) {
            if (std::holds_alternative<MaterialTexture>(val)) {
                auto tex = std::get<MaterialTexture>(val).texID;
                auto imageAsset = am->FindAsset<Texture>(tex);
                initializer.AddPropertyTexture(Name(key.c_str()), CreateTextureFromAsset(imageAsset));
            } else {
                initializer.AddPropertyValue(Name(key.c_str()), val);
            }
        }
        initializer.Finalize();

        mat->SetProperties(std::move(initializer));
        return mat;
    }

    CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(const MaterialInstanceAssetPtr &asset)
    {
        SKY_PROFILE_NAME("Create Material Instance From Asset")
        auto &data = asset->Data();

        auto *am = AssetManager::Get();
        auto matAsset = am->FindAsset<Material>(data.material);
        auto *mi = new MaterialInstance();
        mi->SetMaterial(CreateMaterialFromAsset(matAsset));
        for (const auto &[key, val] : data.properties.valueMap) {
            std::visit(Overloaded{
                [&](const MaterialTexture &v) {
                    auto imageAsset = am->FindAsset<Texture>(v.texID);
                    mi->SetTexture(Name(key.c_str()), CreateTextureFromAsset(imageAsset));
                },
                [&](const auto &v) {
                    mi->SetValue(Name(key.c_str()), v);
                },
            }, val);
        }
        return mi;
    }
} // namespace sky