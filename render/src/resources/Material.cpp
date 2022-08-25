//
// Created by Zach Lee on 2022/5/7.
//


#include <render/resources/Material.h>
#include <framework/asset/AssetManager.h>
#include <cereal/archives/json.hpp>
#include <render/Render.h>

namespace sky {

    void Material::AddGfxTechnique(const RDGfxTechniquePtr& tech)
    {
        gfxTechniques.emplace_back(tech);
    }

    const std::vector<RDGfxTechniquePtr>& Material::GetGraphicTechniques() const
    {
        return gfxTechniques;
    }

    void Material::InitRHI()
    {
        if (gfxTechniques.empty()) {
            return;
        }
        auto& tech = gfxTechniques[0];
        auto shaderTable = tech->GetShaderTable();
        matSet = shaderTable->CreateDescriptorGroup(2);
        auto& descriptorTable = matSet->GetRHISet()->GetLayout()->GetDescriptorTable();

        Buffer::Descriptor descriptor = {};
        descriptor.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        descriptor.allocCPU = true;
        descriptor.size = 0;

        materialBuffer = std::make_shared<Buffer>();
        auto defaultTexture = Render::Get()->GetDefaultTexture();
        for (auto& [binding, info] : descriptorTable) {
            if (info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                bufferViews[binding] = std::make_shared<BufferView>(materialBuffer, info.size, descriptor.size);
                descriptor.size += info.size;
            } else if (info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                textures[binding] = defaultTexture;
            }
        }
        materialBuffer->Init(descriptor);
        materialBuffer->InitRHI();

        for (auto& [binding, view] : bufferViews) {
            matSet->UpdateBuffer(binding, view);
        }

        for (auto& [binding, tex] : textures) {
            matSet->UpdateTexture(binding, tex);
        }
        matSet->Update();
    }

    RDDesGroupPtr Material::GetMaterialSet() const
    {
        return matSet;
    }

    void Material::Update()
    {
        for (auto& view : bufferViews) {
            view.second->RequestUpdate();
        }
        matSet->Update();
    }

    namespace impl {
        void LoadFromPath(const std::string& path, MaterialAssetData& data)
        {
            auto realPath = AssetManager::Get()->GetRealPath(path);
            std::ifstream file(realPath,  std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::JSONInputArchive archive(file);
            archive >> data;
        }

        void SaveToPath(const std::string& path, const MaterialAssetData& data)
        {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::JSONOutputArchive binOutput(file);
            binOutput << data;
        }

        RDMaterialPtr CreateFromData(const MaterialAssetData& data)
        {
            return {};
        }
    }
}