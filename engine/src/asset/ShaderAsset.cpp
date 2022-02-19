//
// Created by Zach Lee on 2022/1/16.
//

#include <engine/asset/ShaderAsset.h>
#include <engine/render/DriverManager.h>
#include <framework/asset/ResourceManager.h>
#include <spirv_cross/spirv_cross.hpp>

namespace sky {

//    AssetBase* ShaderAssetHandler::Create(const Uuid& id)
//    {
//        return new ShaderAsset(id);
//    }
//
//    AssetBase* ShaderAssetHandler::Load(const std::string& uri)
//    {
//        auto shaderAsset = Create(Uuid::Create());
//        auto* source = static_cast<ShaderAsset*>(shaderAsset);
//        ShaderLoader loader;
//        if (!loader.Load(uri, source->sourceData)) {
//            return nullptr;
//        }
//        return shaderAsset;
//    }

    CounterPtr<Shader> Shader::CreateFromAsset(AssetPtr asset)
    {
        if (!asset) {
            return {};
        }
        auto instance = ResourceManager::Get()->GetOrCreate<Shader>(asset->GetId());
        auto device = DriverManager::Get()->GetDevice();

        auto shaderAsset = static_cast<ShaderAsset*>(asset.Get());

        instance->program.shaders.reserve(shaderAsset->sourceData.shaders.size());

        drv::PipelineLayout::Descriptor desc = {};
        for (auto& source : shaderAsset->sourceData.shaders) {
            drv::Shader::Descriptor des = {};
            des.stage = source.stage;
            des.spv = source.data.data();
            des.size = static_cast<uint32_t>(source.data.size() * sizeof(uint32_t));
            auto drvShader = device->CreateDeviceObject<drv::Shader>(des);
            if (!drvShader) {
                return {};
            }
            instance->program.shaders.emplace_back(drv::GraphicsPipeline::ShaderInfo {
                drvShader, source.entry
            });

//            spirv_cross::Compiler glsl(source.data);
//            spirv_cross::ShaderResources resources = glsl.get_shader_resources();
//            for (auto &resource : resources.sampled_images)
//            {
//                unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
//                unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
//            }

        }
        return instance;
    }

    const drv::GraphicsPipeline::Program& Shader::GetProgram() const
    {
        return program;
    }

    const drv::GraphicsPipeline::State& Shader::GetState() const
    {
        return state;
    }

}