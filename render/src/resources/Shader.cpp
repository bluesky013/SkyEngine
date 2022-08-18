//
// Created by Zach Lee on 2022/5/7.
//

#include <render/resources/Shader.h>
#include <render/DriverManager.h>
#include <core/file/FileIO.h>
#include <render/GlobalDescriptorPool.h>

#include <spirv_cross/spirv_cross.hpp>

namespace sky {

    Shader::Shader(const Descriptor& desc) : descriptor(desc)
    {
    }

    void Shader::LoadData(const std::string& path)
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

    drv::ShaderPtr Shader::GetShader() const
    {
        return rhiShader;
    }

    const Shader::DescriptorTable& Shader::GetDescriptorTable() const
    {
        return descriptorTable;
    }

    const Shader::StageInputTable& Shader::GetStageInputs() const
    {
        return stageInputs;
    }

    const Shader::NameTable& Shader::GetNameTable() const
    {
        return nameTable;
    }

    void Shader::BuildReflection()
    {
        spirv_cross::Compiler compiler(spv.data(), spv.size());

        using SpvResources = spirv_cross::SmallVector<spirv_cross::Resource>;
        auto fn = [&compiler, this](const SpvResources& resources, VkDescriptorType type) {
            for (auto& res : resources) {
                auto set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                auto binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                auto& name = compiler.get_name(res.id);

                auto& entry = descriptorTable[set].bindings[binding];
                entry.descriptorType = type;
                entry.stageFlags = descriptor.stage;
                entry.descriptorCount = 1;

                auto& table = nameTable[set];
                if (!table) {
                    table = std::make_shared<ProperTableInfo>();
                }

                if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                    auto& type = compiler.get_type(res.base_type_id);
                    size_t memberNum = type.member_types.size();
                    for (uint32_t i = 0; i < memberNum; ++i) {
                        uint32_t offset = compiler.get_member_decoration(res.base_type_id, i, spv::DecorationOffset);
                        std::string nameKey = name + "." + compiler.get_member_name(res.base_type_id, i);
                        table->handleMap.emplace(nameKey, PropertyHandler{ binding, offset });
                        entry.size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
                    }
                } else {
                    table->handleMap.emplace(name, PropertyHandler{ binding, 0 });
                }
            }
        };

        auto resources = compiler.get_shader_resources();

        fn(resources.uniform_buffers, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        fn(resources.storage_buffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        fn(resources.sampled_images, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        fn(resources.storage_images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        if (descriptor.stage == VK_SHADER_STAGE_VERTEX_BIT) {
            auto fn = [&compiler, this](const SpvResources& resources) {
                for (auto& res : resources) {
                    auto location = compiler.get_decoration(res.id, spv::DecorationLocation);
                    auto& name = compiler.get_name(res.id);
                    auto& info = stageInputs[name];
                    info.location = location;
                    auto& type = compiler.get_type(res.type_id);
                    if (type.basetype == spirv_cross::SPIRType::Float) {
                        if (type.vecsize == 4) {
                            info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                        } if (type.vecsize == 3) {
                            info.format = VK_FORMAT_R32G32B32_SFLOAT;
                        } if (type.vecsize == 2) {
                            info.format = VK_FORMAT_R32G32_SFLOAT;
                        }
                    }
                }
            };
            fn(resources.stage_inputs);
        }
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
                    merge.size = binding.second.size;
                }
            }
            auto& shaderNameTable = shader->GetNameTable();
            for (auto& pair : shaderNameTable) {
                for (auto& [name, handler] : pair.second->handleMap) {
                    auto& handleMap = nameTable[pair.first];
                    if (!handleMap) {
                        handleMap = std::make_shared<ProperTableInfo>();
                    }

                    nameTable[pair.first]->handleMap.emplace(name, handler);
                }
            }
        }
        auto device = DriverManager::Get()->GetDevice();
        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(desc);
    }

    drv::PipelineLayoutPtr ShaderTable::GetPipelineLayout() const
    {
        return pipelineLayout;
    }

    RDDesGroupPtr ShaderTable::CreateDescriptorGroup(uint32_t slot) const
    {
        auto iter = nameTable.find(slot);
        if (iter == nameTable.end()) {
            return {};
        }

        drv::DescriptorSetLayoutPtr layout = pipelineLayout->GetLayout(slot);
        if (!layout) {
            return {};
        }

        RDDescriptorPoolPtr pool = GlobalDescriptorPool::Get()->GetPool(layout);
        RDDesGroupPtr res = pool->Allocate();
        res->SetPropertyTable(iter->second);
        return res;
    }

    void ShaderTable::FillProgram(drv::GraphicsPipeline::Program& program)
    {
        for (auto& shader : shaders) {
            program.shaders.emplace_back(shader->GetShader());
        }
    }

    void GraphicsShaderTable::LoadShader(const std::string &vsPath, const std::string &fsPath)
    {
        vs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_VERTEX_BIT});
        vs->LoadData(vsPath);
        vs->InitRHI();

        fs = std::make_shared<Shader>(Shader::Descriptor{VK_SHADER_STAGE_FRAGMENT_BIT});
        fs->LoadData(fsPath);
        fs->InitRHI();

        shaders.emplace_back(vs);
        shaders.emplace_back(fs);
    }

    bool GraphicsShaderTable::IsValid() const
    {
        return vs && fs;
    }
}