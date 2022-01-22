//
// Created by Zach Lee on 2022/1/16.
//

#include <engine/asset/ShaderAsset.h>
#include <engine/loader/shader/ShaderLoader.h>
#include <engine/render/DriverManager.h>
#include <framework/asset/ResourceManager.h>

namespace sky {

    AssetPtr ShaderAssetHandler::Create(const Uuid& id)
    {
        return new ShaderAsset(id);
    }

    AssetPtr ShaderAssetHandler::Load(const std::string& uri)
    {
        auto shaderAsset = new ShaderAsset(Uuid::Create());
        ShaderLoader loader;
        loader.Load(uri, shaderAsset->sourceData);
        return shaderAsset;
    }

    CounterPtr<Shader> Shader::CreateFromAsset(AssetPtr asset)
    {
        auto instance = ResourceManager::Get()->GetOrCreate<Shader>(asset->GetId());
        auto device = DriverManager::Get()->GetDevice();

        auto shaderAsset = static_cast<ShaderAsset*>(asset.Get());

        instance->shaders.reserve(shaderAsset->sourceData.shaders.size());
        for (auto& source : shaderAsset->sourceData.shaders) {
            drv::Shader::Descriptor des = {};
            des.stage = source.stage;
            des.spv = source.data.data();
            des.size = static_cast<uint32_t>(source.data.size() * sizeof(uint32_t));
            instance->shaders.emplace_back(device->CreateDeviceObject<drv::Shader>(des));
        }
        return instance;
    }


}