//
// Created by Zach Lee on 2022/5/7.
//


#include <render/resources/Material.h>
#include <render/GlobalDescriptorPool.h>

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
        drv::PipelineLayoutPtr pipelineLayout = shaderTable->GetPipelineLayout();
        drv::DescriptorSetLayoutPtr setLayout = pipelineLayout->GetLayout(2);
        RDDescriptorPoolPtr pool = GlobalDescriptorPool::Get()->GetPool(setLayout);
        matSet = pool->Allocate();

        struct Temp {
            float value[4];
        };
        Temp val = {1.f, 0.f, 0.f, 1.f};

        Buffer::Descriptor descriptor = {};
        descriptor.size = sizeof(Temp);
        descriptor.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        descriptor.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        descriptor.allocCPU = true;
        materialBuffer = std::make_shared<Buffer>(descriptor);
        materialBuffer->InitRHI();

        materialBuffer->Write(val, 0);
        materialBuffer->Update();

        auto view = std::make_shared<BufferView>(materialBuffer, sizeof(Temp), 0);
        matSet->UpdateBuffer(0, view);
        matSet->Update();
    }

    RDDesGroupPtr Material::GetMaterialSet() const
    {
        return matSet;
    }
}