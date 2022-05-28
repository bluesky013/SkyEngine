//
// Created by Zach Lee on 2022/5/7.
//

#include <engine/render/resources/Shader.h>
#include <engine/render/DriverManager.h>
#include <core/file/FileIO.h>

#include <spirv_cross/spirv_cross.hpp>

namespace sky {

    Shader::Shader(const Descriptor& desc) : descriptor(desc)
    {
    }

    void Shader::LoadFromFile(const std::string& path)
    {
        ReadBin(path, spv);
    }

    void Shader::SetData(std::vector<uint32_t>&& data)
    {
        spv.swap(data);
    }

    void Shader::InitRHI()
    {
        BuildReflection();

        drv::Shader::Descriptor desc = {};
        desc.size = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        desc.spv = spv.data();
        desc.stage = descriptor.stage;
        rhiShader = DriverManager::Get()->CreateDeviceObject<drv::Shader>(desc);
    }

    bool Shader::IsValid() const
    {
        return !!rhiShader;
    }

    const Shader::DescriptorTable& Shader::GetDescriptorTable() const
    {
        return descriptorTable;
    }

    void Shader::BuildReflection()
    {
        spirv_cross::Compiler compiler(spv.data(), spv.size());

        auto fn = [&compiler, this](spirv_cross::SmallVector<spirv_cross::Resource>& resources, VkDescriptorType type) {
            for (auto& res : resources) {
                auto set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(res.id, spv::DecorationBinding);

                auto& entry = descriptorTable[set].bindings[binding];
                entry.descriptorType = type;
                entry.stageFlags = descriptor.stage;
                entry.descriptorCount = 1;
            }
        };

        auto resources = compiler.get_shader_resources();

        fn(resources.uniform_buffers, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        fn(resources.storage_buffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        fn(resources.sampled_images, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        fn(resources.storage_images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }

    void ShaderTable::InitRHI()
    {
        if (pipelineLayout) {
            return;
        }

        drv::PipelineLayout::Descriptor desc = {};
        for (auto& shader : shaders) {
            auto& table = shader->GetDescriptorTable();
            for (auto& item : table) {
                if (item.first >= desc.desLayouts.size()) {
                    desc.desLayouts.resize(item.first + 1);
                }
                auto& entry = desc.desLayouts[item.first];
                for (auto& binding : item.second.bindings) {
                    auto& merge = entry.bindings[binding.first];
                    merge.stageFlags |= binding.second.stageFlags;
                    merge.descriptorType = binding.second.descriptorType;
                    merge.descriptorCount = binding.second.descriptorCount;
                }
            }
        }

        pipelineLayout = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::PipelineLayout>(desc);
    }

    void GraphicsShaderTable::LoadFromFile(const std::string &vsPath, const std::string &fsPath)
    {
        vs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_VERTEX_BIT});
        vs->LoadFromFile(vsPath);
        vs->InitRHI();

        fs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_FRAGMENT_BIT});
        fs->LoadFromFile(fsPath);
        fs->InitRHI();

        shaders.emplace_back(vs);
        shaders.emplace_back(fs);
    }

    bool GraphicsShaderTable::IsValid() const
    {
        return vs && fs;
    }

}