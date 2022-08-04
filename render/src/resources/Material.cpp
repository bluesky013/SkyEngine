//
// Created by Zach Lee on 2022/5/7.
//


#include <render/resources/Material.h>

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
        for (auto& [binding, info] : descriptorTable) {
            if (info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                info.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                bufferView[binding] = std::make_shared<BufferView>(materialBuffer, info.size, descriptor.size);
                descriptor.size += info.size;
            }
        }
        materialBuffer->Init(descriptor);
        materialBuffer->InitRHI();

        for (auto& [binding, view] : bufferView) {
            matSet->UpdateBuffer(binding, view);
        }
        matSet->Update();
    }

    RDDesGroupPtr Material::GetMaterialSet() const
    {
        return matSet;
    }

    void Material::Update()
    {
        for (auto& view : bufferView) {
            view.second->RequestUpdate();
        }
    }
}