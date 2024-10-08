//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Material.h>
#include <render/RHI.h>
#include <render/Renderer.h>

namespace sky {
    static constexpr uint32_t MAX_SET_PER_POOL = 8;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL},
        {rhi::DescriptorType::SAMPLED_IMAGE,          6 * MAX_SET_PER_POOL},
        {rhi::DescriptorType::SAMPLER,                6 * MAX_SET_PER_POOL},
    };

    void Material::AddTechnique(const RDGfxTechPtr &technique)
    {
        gfxTechniques.emplace_back(technique);
        if (!pool) {
            rhi::DescriptorSetPool::Descriptor poolDesc = {};

            poolDesc.maxSets   = MAX_SET_PER_POOL;
            poolDesc.sizeCount = static_cast<uint32_t>(SIZES.size());
            poolDesc.sizeData  = SIZES.data();
            pool               = RHI::Get()->GetDevice()->CreateDescriptorSetPool(poolDesc);
        }
    }

    RDResourceGroupPtr Material::RequestResourceGroup(const RDResourceLayoutPtr &layout)
    {
        if (!layout) {
            return {};
        }

        auto *rsg = new ResourceGroup();
        rsg->Init(layout, *pool);
        return rsg;
    }

    MaterialInstance::MaterialInstance()
        : options(new ShaderOption())
    {
    }

    void MaterialInstance::SetMaterial(const RDMaterialPtr &mat)
    {
        material = mat;
    }

    void MaterialInstance::SetValue(const std::string &key, const uint8_t *t, uint32_t size)
    {
        if (!resourceGroup) {
            return;
        }

        const auto *handler = resourceGroup->GetLayout()->GetBufferMemberByName(key);
        if (handler != nullptr) {
            uint32_t index = resourceGroup->GetLayout()->GetRHILayout()->GetDescriptorSetOffsetByBinding(handler->binding);
            auto &ubo = uniformBuffers.at(index);
            ubo->Write(handler->offset, t, size);
        }
    }

    void MaterialInstance::SetOption(const std::string &key, const MacroValue &val)
    {
        options->SetValue(key, val);
    }

    void MaterialInstance::SetTexture(const std::string &key, const RDTexturePtr &tex, uint32_t index)
    {
        if (!resourceGroup) {
            return;
        }

        const auto *handler = resourceGroup->GetLayout()->GetBindingByeName(key);
        if (handler != nullptr) {
            uint32_t offset = resourceGroup->GetLayout()->GetRHILayout()->GetDescriptorSetOffsetByBinding(handler->binding);
            resourceGroup->BindTexture(key, tex->GetImageView(), index);
            textures[offset + index] = tex;
            resDirty = true;
        }
    }

    void MaterialInstance::Compile()
    {
        const auto &tech = material->GetGfxTechniques()[0];
        auto program = tech->RequestProgram(options);
        resourceGroup = material->RequestResourceGroup(program->RequestLayout(BATCH_SET));
        if (!resourceGroup) {
            return;
        }

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
                    auto *ubo = new DynamicUniformBuffer();
                    ubo->Init(pair.second.size);
                    uniformBuffers.emplace(index + i, ubo);
                    resourceGroup->BindDynamicUBO(pair.first, ubo, i);
                }
            }
        }
    }

    void MaterialInstance::Upload()
    {
        if (uploaded) {
            return;
        }
        auto *sm = Renderer::Get()->GetStreamingManager();
        for (auto &[idx, tex] : textures) {
            sm->UploadTexture(tex);
        }
        uploaded = true;
    }

    void MaterialInstance::Update()
    {
        for (auto &ubo : uniformBuffers) {
            ubo.second->Upload();
        }

        if (resDirty && resourceGroup) {
            resourceGroup->Update();
            resDirty = false;
        }
    }

    bool MaterialInstance::IsReady() const
    {
        return !std::any_of(textures.begin(), textures.end(), [](const auto &tex) {
            return !tex.second->IsReady();
        });
    }

    void MaterialInstance::ProcessShaderOption(const ShaderOptionPtr &outOptions) const
    {
        for (const auto &[key, val] : options->values) {
            outOptions->SetValue(key, val);
        }
    }

} // namespace sky
