//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Material.h>
#include <render/RHI.h>
#include <render/Renderer.h>

namespace sky {

    static constexpr uint32_t BATCH_SET = 1;
    static constexpr uint32_t MAX_SET_PER_POOL = 8;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL},
        {rhi::DescriptorType::COMBINED_IMAGE_SAMPLER, 6 * MAX_SET_PER_POOL},
    };

    void Material::AddTechnique(const RDGfxTechPtr &technique)
    {
        gfxTechniques.emplace_back(technique);

        if (!layout) {
            auto program = gfxTechniques.back()->RequestProgram();
            auto rhiLayout = program->GetPipelineLayout()->GetSetLayout(BATCH_SET);
            if (rhiLayout) {
                layout = std::make_shared<ResourceGroupLayout>();
                layout->SetRHILayout(rhiLayout);

                for (const auto &shader : program->GetShaders()) {
                    const auto &shaderResources = shader->GetShaderResources();
                    for (const auto &shaderResource : shaderResources) {
                        if (shaderResource.set != BATCH_SET) {
                            continue;
                        }

                        layout->AddNameHandler(shaderResource.name, {shaderResource.binding, shaderResource.size});
                        for (const auto &member : shaderResource.members) {
                            layout->AddBufferNameHandler(member.name, {shaderResource.binding, member.offset, member.size});
                        }
                    }
                }
            }

            if (!pool) {
                rhi::DescriptorSetPool::Descriptor poolDesc = {};

                poolDesc.maxSets   = MAX_SET_PER_POOL;
                poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
                poolDesc.sizeData  = SIZES.data();
                pool               = RHI::Get()->GetDevice()->CreateDescriptorSetPool(poolDesc);
            }

        }
    }

    RDResourceGroupPtr Material::RequestResourceGroup()
    {
        auto rsg = std::make_shared<ResourceGroup>();
        rsg->Init(layout, *pool);
        return rsg;
    }

    void MaterialInstance::SetMaterial(const RDMaterialPtr &mat)
    {
        material = mat;
        resourceGroup = material->RequestResourceGroup();

        const auto &defaultRes = Renderer::Get()->GetDefaultRHIResource();

        const auto &bindingHandlers = resourceGroup->GetLayout()->GetBindingHandlers();
        const auto &rhiLayout = resourceGroup->GetLayout()->GetRHILayout();
        const auto &bindings = rhiLayout->GetBindings();
        for (const auto &pair : bindingHandlers) {
            uint32_t index = rhiLayout->GetDescriptorSetOffsetByBinding(pair.second.binding);
            uint32_t count = rhiLayout->GetDescriptorSetCountByBinding(pair.second.binding);
            auto iter = std::find_if(bindings.begin(), bindings.end(), [&pair](const auto &val) {
                return pair.second.binding == val.binding;
            });
            SKY_ASSERT(iter != bindings.end());
            for (uint32_t i = 0; i < count; ++i) {
                if (iter->type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC || iter->type == rhi::DescriptorType::UNIFORM_BUFFER) {
                    auto ubo = std::make_shared<DynamicUniformBuffer>();
                    ubo->Init(pair.second.size, Renderer::Get()->GetInflightFrameCount());
                    uniformBuffers.emplace(index + i, ubo);
                    resourceGroup->BindDynamicUBO(pair.first, ubo, i);
                } else if (iter->type == rhi::DescriptorType::COMBINED_IMAGE_SAMPLER) {
                    textures.emplace(index + i, defaultRes.texture2D);
                    resourceGroup->BindTexture(pair.first, defaultRes.texture2D->GetImageView(), i);
                }
            }
        }
    }

    void MaterialInstance::SetValue(const std::string &key, const uint8_t *t, uint32_t size)
    {
        const auto *handler = resourceGroup->GetLayout()->GetBufferMemberByName(key);
        if (handler != nullptr) {
            uint32_t index = resourceGroup->GetLayout()->GetRHILayout()->GetDescriptorSetOffsetByBinding(handler->binding);
            auto &ubo = uniformBuffers.at(index);
            ubo->Write(handler->offset, t, size);
        }
    }

    void MaterialInstance::SetTexture(const std::string &key, const RDTexturePtr &tex, uint32_t index)
    {
        const auto *handler = resourceGroup->GetLayout()->GetBufferMemberByName(key);
        if (handler != nullptr) {
            textures[handler->binding + index] = tex;
            resDirty = true;
        }
    }

    void MaterialInstance::Upload()
    {
        for (auto &ubo : uniformBuffers) {
            ubo.second->Upload();
        }

        if (resDirty) {
            resourceGroup->Update();
            resDirty = false;
        }
    }

} // namespace sky