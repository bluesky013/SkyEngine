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
            archive.LoadValue(properties.images[i]);
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
            archive.SaveValue(image);
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
            if (val.Info()->typeId == TypeInfo<MaterialTexture>::Hash()) {
                auto tex = val.GetAsConst<MaterialTexture>()->texIndex;
                auto imageAsset = am->LoadAsset<Texture>(data.properties.images[tex]);
                mi->SetTexture(key, imageAsset->CreateInstance(), 0);
            } else {
                mi->SetValue(key, static_cast<const uint8_t*>(val.Data()), static_cast<uint32_t>(val.Info()->size));
            }
        }
        mi->Upload();
        return mi;
    }
} // namespace sky