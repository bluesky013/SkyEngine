//
// Created by Zach Lee on 2023/2/23.
//

#include <framework/asset/AssetManager.h>
#include <framework/serialization/BinaryArchive.h>
#include <render/adaptor/assets/ShaderAsset.h>
#include <render/RHI.h>

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
        uint32_t size = 0;
        archive.LoadValue(size);
        for (uint32_t i = 0; i < size; ++i) {
            std::string key;
            archive.LoadValue(key);
            std::string uuid;
            archive.LoadValue(uuid);
            variants.emplace(key, AssetManager::Get()->LoadAsset<ShaderVariant>(Uuid::CreateFromString(uuid)));
        }
        uint32_t tmp = 0;
        archive.LoadValue(tmp);
        stage = static_cast<rhi::ShaderStageFlagBit>(tmp);
    }

    void ShaderAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(variants.size()));
        for (const auto &[key, var] : variants) {
            archive.SaveValue(key);
            archive.SaveValue(var->GetUuid().ToString());
        }
        archive.SaveValue(static_cast<uint32_t>(stage));
    }

    std::shared_ptr<Shader> CreateShader(const ShaderAssetData &data)
    {
        auto shader = std::make_shared<Shader>();

        auto api = RHI::Get()->GetBackend();
        for (const auto &[key, variant] : data.variants) {
            auto sv = std::make_shared<ShaderVariant>();
            auto &svData = variant->Data();
            if (api == rhi::API::VULKAN) {
                sv->Init(data.stage, reinterpret_cast<const uint8_t *>(svData.spv.data()), static_cast<uint32_t>(svData.spv.size() * sizeof(uint32_t)));
            }
            shader->AddVariant(key, sv);
        }
        return shader;
    }
}