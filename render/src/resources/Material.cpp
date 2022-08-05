//
// Created by Zach Lee on 2022/5/7.
//


#include <render/resources/Material.h>
#include <render/Render.h>

namespace sky {

    void Material::AddGfxTechnique(RDGfxTechniquePtr tech)
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
        auto tex = Render::Get()->GetDefaultTexture();
        for (auto& [binding, info] : descriptorTable) {
            if (info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                bufferViews[binding] = std::make_shared<BufferView>(materialBuffer, info.size, descriptor.size);
                descriptor.size += info.size;
            } else if (info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                textures[binding] = tex;
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
}