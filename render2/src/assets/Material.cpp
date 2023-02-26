//
// Created by Zach Lee on 2023/2/23.
//
#include <render/assets/Material.h>

namespace sky {
    void MaterialAssetData::LoadBin(BinaryInputArchive &archive)
    {

    }

    void MaterialAssetData::SaveBin(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(techniques.size()));
        for (auto &tech :techniques) {
            archive.SaveValue(tech ? tech->GetUuid().ToString() : Uuid{}.ToString());
        }

        archive.SaveValue(static_cast<uint32_t>(valueMap.size()));
        for (auto &[key, value] : valueMap) {
            archive.SaveValue(key);
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