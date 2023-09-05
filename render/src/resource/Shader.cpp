//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Shader.h>
#include <render/RHI.h>
#include <render/Renderer.h>

namespace sky {

    bool ShaderVariant::Init(rhi::ShaderStageFlagBit stage_, const uint8_t *data, uint32_t size)
    {
        stage = stage_;

        rhi::Shader::Descriptor desc = {};
        desc.size = size;
        desc.data = data;
        desc.stage = stage;

        shader = RHI::Get()->GetDevice()->CreateShader(desc);
        return static_cast<bool>(shader);
    }

    void ShaderVariant::SetShaderResources(const std::vector<ShaderResource> &res)
    {
        resources = res;
    }

    const ShaderVariantPtr &Shader::GetVariant(const std::string &key) const
    {
        return variants.at(key);
    }

    void Shader::AddVariant(const std::string &key, const ShaderVariantPtr &variant)
    {
        variants.emplace(key, variant);
    }

    void Program::AddShader(const ShaderVariantPtr &shader)
    {
        shaders.emplace_back(shader);
    }

    void Program::BuildPipelineLayout()
    {
        std::vector<rhi::DescriptorSetLayout::Descriptor> layoutDesc;
        for (auto &shader : shaders) {
            const auto &resources = shader->GetShaderResources();
            for (auto &res : resources) {
                if (res.set >= layoutDesc.size()) {
                    layoutDesc.resize(res.set + 1);
                }

                auto &bindings = layoutDesc[res.set].bindings;
                auto iter = std::find_if(bindings.begin(), bindings.end(), [&res](const auto &bd) {
                    return res.binding == bd.binding;
                });
                if (iter != bindings.end()) {
                    iter->visibility |= shader->GetStage();
                } else {
                    rhi::DescriptorSetLayout::SetBinding binding = {};
                    binding.name = res.name;
                    binding.type = res.type;
                    binding.count = 1;
                    binding.binding = res.binding;
                    binding.visibility = shader->GetStage();
                    bindings.emplace_back(binding);
                }
            }
        }

        auto *device = RHI::Get()->GetDevice();
        rhi::PipelineLayout::Descriptor plDesc = {};
        for (auto &desc : layoutDesc) {
            if (desc.bindings.empty()) {
                plDesc.layouts.emplace_back(Renderer::Get()->GetDefaultRHIResource().emptyDesLayout);
            } else {
                plDesc.layouts.emplace_back(device->CreateDescriptorSetLayout(desc));
            }
        }
        pipelineLayout = device->CreatePipelineLayout(plDesc);
    }

} // namespace sky
