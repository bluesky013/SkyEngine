//
// Created by Zach Lee on 2023/2/23.
//

#include <framework/serialization/SerializationContext.h>
#include <render/adaptor/assets/MaterialAsset.h>

namespace sky {
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

        size = 0;
        archive.LoadValue(size);
        images.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string idStr;
            archive.LoadValue(idStr);
            images[i] = AssetManager::Get()->LoadAsset<Texture>(Uuid::CreateFromString(idStr));
        }

        size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            uint32_t typeId = 0;
            archive.LoadValue(typeId);
            valueMap.emplace(key, MakeAny(typeId));
        }
    }

    void MaterialAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(techniques.size()));
        for (auto &tech :techniques) {
            archive.SaveValue(tech ? tech->GetUuid().ToString() : Uuid{}.ToString());
        }

        archive.SaveValue(static_cast<uint32_t>(images.size()));
        for (auto &image : images) {
            archive.SaveValue(image ? image->GetUuid().ToString() : Uuid{}.ToString());
        }

        archive.SaveValue(static_cast<uint32_t>(valueMap.size()));
        for (auto &[key, value] : valueMap) {
            archive.SaveValue(key);
            archive.SaveValue(value.Info()->typeId);
            archive.SaveObject(value.Data(), value.Info()->typeId);
        }
    }

    void MaterialAssetData::LoadJson(JsonInputArchive &archive)
    {
    }

    void MaterialAssetData::SaveJson(JsonOutputArchive &archive) const
    {
        archive.StartObject();

        archive.Key("techniques");
        archive.StartArray();
        for (auto &tech : techniques) {
            archive.SaveValue(tech->GetUuid().ToString());
        }
        archive.EndArray();

        archive.Key("images");
        archive.StartArray();
        for (auto &image : images) {
            archive.SaveValue(image->GetUuid().ToString());
        }

        archive.EndArray();

        archive.Key("values");
        archive.StartObject();

        for (auto &[key,value] : valueMap) {
            archive.Key(key.c_str());
            archive.SaveValueObject(value);
        }

        archive.EndObject();

        archive.EndObject();
    }


}