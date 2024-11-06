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

        size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            MaterialValueType type;
            archive.LoadValue(type);
            switch (type) {
                case MaterialValueType::BOOL: {
                    bool val = {};
                    archive.LoadValue(val);
                    properties.options[key] = val;
                }
                break;
                case MaterialValueType::U32: {
                    uint32_t val = {};
                    archive.LoadValue(val);
                    properties.options[key] = val;
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

        archive.SaveValue(static_cast<uint32_t>(properties.options.size()));
        for (const auto &[key, value]: properties.options) {
            archive.SaveValue(key);

            std::visit(Overloaded{
                    [&archive](const bool &v) {
                        archive.SaveValue(MaterialValueType::BOOL);
                        archive.SaveValue(v);
                    },
                    [&archive](const uint32_t &v) {
                        archive.SaveValue(MaterialValueType::U32);
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

        // std::variant<MaterialTexture, Vector2, Vector3, Vector4, float, uint32_t, int32_t>;
        archive.ForEachMember([this, &archive](const std::string &key, const auto &obj) {
            if (obj.IsString()) {
                auto value = Uuid::CreateFromString(obj.GetString());
                auto iter = std::find(images.begin(), images.end(), value);
                if (iter != images.end()) {
                    valueMap[key] = MaterialTexture{static_cast<uint32_t>(std::distance(iter, images.begin()))};
                } else {
                    valueMap[key] = MaterialTexture{static_cast<uint32_t>(images.size())};
                    images.emplace_back(value);
                }

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

        archive.Start("options");
        archive.ForEachMember([this, &archive](const std::string &key, const auto &obj) {
            if (obj.IsBool()) {
                options[key] = obj.GetBool();
            } else if (obj.IsUint()) {
                options[key] = obj.GetUint();
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

            // std::variant<MaterialTexture, Vector2, Vector3, Vector4, float, uint32_t, int32_t>;
            std::visit(Overloaded{
                    [&archive, this](const MaterialTexture &tex) {
                        auto asset = AssetDataBase::Get()->FindAsset(images[tex.texIndex]);
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

        archive.Key("options");
        archive.StartObject();
        for (const auto &[key, value] : options) {
            archive.Key(key.c_str());
            std::visit(Overloaded{
                    [&archive](const bool &val) {
                        archive.SaveValue(val);
                    },
                    [&archive](const uint32_t &val) {
                        archive.SaveValue(val);
                    },
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
        return mat;
    }

    CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(const MaterialInstanceAssetPtr &asset)
    {
        SKY_PROFILE_NAME("Create Material Instance From Asset")
        auto &data = asset->Data();

        auto *am = AssetManager::Get();
        auto matAsset = am->FindAsset<Material>(data.material);
        auto *mi = new MaterialInstance();
//        mi->SetName(asset->GetName());
        mi->SetMaterial(CreateMaterialFromAsset(matAsset));
        for (const auto &[key, val] : data.properties.options) {
            mi->SetOption(key, val);
        }
        mi->Compile();

        for (const auto &[key, val] : data.properties.valueMap) {
            std::visit(Overloaded{
                    [&mi, &data, &am, key_ = key](const MaterialTexture &v) {
                        auto imageAsset = am->FindAsset<Texture>(data.properties.images[v.texIndex]);
                        mi->SetTexture(key_, CreateTextureFromAsset(imageAsset), 0);
                    },
                    [&mi, key_ = key](const auto &v) {
                        mi->SetValue(key_, reinterpret_cast<const uint8_t*>(&v), sizeof(decltype(v)));
                    }
            }, val);
        }
        mi->Update();
        return mi;
    }
} // namespace sky