//
// Created by Zach Lee on 2023/2/23.
//

#include <render/assets/Shader.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void ShaderVariantData::Load(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        gles.resize(size);
        archive.LoadValue(reinterpret_cast<char *>(gles.data()), size);

        size = 0;
        archive.LoadValue(size);
        spv.resize(size / sizeof(uint32_t));
        archive.LoadValue(reinterpret_cast<char *>(spv.data()), size);
    }

    void ShaderVariantData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(gles);
        auto spvSize = static_cast<uint32_t>(spv.size() * sizeof(uint32_t));
        archive.SaveValue(spvSize);
        archive.SaveValue(reinterpret_cast<const char*>(spv.data()), spvSize);
    }

    void ShaderAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(source);
        uint32_t size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            std::string uuid;
            archive.LoadValue(uuid);
            variants.emplace(key, AssetManager::Get()->LoadAsset<ShaderVariant>(Uuid::CreateFromString(uuid)));
        }
    }

    void ShaderAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(source);
        archive.SaveValue(static_cast<uint32_t>(variants.size()));
        for (auto &[key, var] : variants) {
            archive.SaveValue(key);
            archive.SaveValue(var->GetUuid().ToString());
        }
    }
}