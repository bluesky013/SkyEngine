//
// Created by Zach Lee on 2023/2/23.
//

#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/assets/MaterialAsset.h>

namespace sky {
    void LoadProperties(BinaryInputArchive &archive, MaterialProperties &properties)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        properties.images.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            properties.images[i] = AssetManager::Get()->LoadAsset<Texture>(Uuid::CreateFromString(idStr));
        }

        size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            uint32_t typeId = 0;
            archive.LoadValue(typeId);
            properties.valueMap[key] = MakeAny(typeId);
            archive.LoadObject(properties.valueMap[key].Data(), typeId);
        }
    }

    void SaveProperties(BinaryOutputArchive &archive, const MaterialProperties &properties)
    {
        archive.SaveValue(static_cast<uint32_t>(properties.images.size()));
        for (const auto &image : properties.images) {
            archive.SaveValue(image ? image->GetUuid().ToString() : Uuid{}.ToString());
        }

        archive.SaveValue(static_cast<uint32_t>(properties.valueMap.size()));
        for (const auto &[key, value] : properties.valueMap) {
            archive.SaveValue(key);
            archive.SaveValue(value.Info()->typeId);
            archive.SaveObject(value.Data(), value.Info()->typeId);
        }
    }

    void MaterialAssetData::LoadBin(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        techniques.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            techniques[i] = AssetManager::Get()->LoadAsset<Technique>(Uuid::CreateFromString(idStr));
        }

        LoadProperties(archive, defaultProperties);
    }

    void MaterialAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(techniques.size()));
        for (const auto &tech :techniques) {
            archive.SaveValue(tech ? tech->GetUuid().ToString() : Uuid{}.ToString());
        }
        SaveProperties(archive, defaultProperties);
    }

//    void MaterialAssetData::LoadJson(JsonInputArchive &archive)
//    {
//    }
//
//    void MaterialAssetData::SaveJson(JsonOutputArchive &archive) const
//    {
//        archive.StartObject();
//
//        archive.Key("techniques");
//        archive.StartArray();
//        for (const auto &tech : techniques) {
//            archive.SaveValue(tech->GetUuid().ToString());
//        }
//        archive.EndArray();
//
//        archive.Key("images");
//        archive.StartArray();
//        for (const auto &image : images) {
//            archive.SaveValue(image->GetUuid().ToString());
//        }
//
//        archive.EndArray();
//
//        archive.Key("values");
//        archive.StartObject();
//
//        for (const auto &[key,value] : valueMap) {
//            archive.Key(key.c_str());
//            archive.SaveValueObject(value);
//        }
//
//        archive.EndObject();
//
//        archive.EndObject();
//    }

    void MaterialInstanceData::LoadBin(BinaryInputArchive &archive)
    {
        auto *am = AssetManager::Get();
        {
            std::string idStr;
            archive.LoadValue(idStr);
            material = am->LoadAsset<Material>(Uuid::CreateFromString(idStr));
        }

        LoadProperties(archive, properties);
    }

    void MaterialInstanceData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(material->GetUuid().ToString());
        SaveProperties(archive, properties);
    }

    std::shared_ptr<Material> CreateMaterial(const MaterialAssetData &data)
    {
        auto mat = std::make_shared<Material>();
        for (const auto &tech : data.techniques) {
            mat->AddTechnique(tech->CreateInstanceAs<GraphicsTechnique>());
        }
        return mat;
    }

    std::shared_ptr<MaterialInstance> CreateMaterialInstance(const MaterialInstanceData &data)
    {
        auto mi = std::make_shared<MaterialInstance>();
        mi->SetMaterial(data.material->CreateInstance());
        for (const auto &[key, val] : data.properties.valueMap) {
            if (val.Info()->typeId == TypeInfo<MaterialTexture>::Hash()) {
                mi->SetTexture(key, data.properties.images[val.GetAsConst<MaterialTexture>()->texIndex]->CreateInstance(), 0);
            } else {
                mi->SetValue(key, static_cast<const uint8_t*>(val.Data()), static_cast<uint32_t>(val.Info()->size));
            }
        }
        mi->Upload();
        return mi;
    }
} // namespace sky