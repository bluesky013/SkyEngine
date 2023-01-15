//
// Created by Zach Lee on 2022/5/7.
//

#include <core/file/FileIO.h>
#include <render/RHIManager.h>
#include <render/GlobalDescriptorPool.h>
#include <render/resources/Shader.h>

#include <spirv_cross/spirv_cross.hpp>

namespace sky {

    Shader::Shader(const Descriptor &desc) : descriptor(desc)
    {
    }

    void Shader::LoadData(const std::string &path)
    {
        ReadBin(path, spv);
    }

    void Shader::SetData(std::vector<uint32_t> &&data)
    {
        spv.swap(data);
    }

    void Shader::SetData(const std::vector<uint32_t> &data)
    {
        spv = data;
    }

    void Shader::InitRHI()
    {
        BuildReflection();

        vk::Shader::VkDescriptor desc = {};
        desc.size                    = static_cast<uint32_t>(spv.size()) * sizeof(uint32_t);
        desc.spv                     = spv.data();
        desc.stage                   = descriptor.stage;
        rhiShader                    = RHIManager::Get()->GetDevice()->CreateDeviceObject<vk::Shader>(desc);
    }

    bool Shader::IsValid() const
    {
        return !!rhiShader;
    }

    vk::ShaderPtr Shader::GetShader() const
    {
        return rhiShader;
    }

    const Shader::DescriptorTable &Shader::GetDescriptorTable() const
    {
        return descriptorTable;
    }

    const Shader::StageInputTable &Shader::GetStageInputs() const
    {
        return stageInputs;
    }

    const Shader::NameTable &Shader::GetNameTable() const
    {
        return nameTable;
    }

    uint32_t Shader::GetConstantBlockSize() const
    {
        return constantBlockSize;
    }

    void Shader::BuildReflection()
    {
        spirv_cross::Compiler compiler(spv.data(), spv.size());

        using SpvResources = spirv_cross::SmallVector<spirv_cross::Resource>;
        auto fn            = [&compiler, this](const SpvResources &resources, VkDescriptorType type) {
            for (auto &res : resources) {
                auto  set     = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                auto  binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                auto &name    = compiler.get_name(res.id);

                auto &entry           = descriptorTable[set].bindings[binding];
                entry.descriptorType  = type;
                entry.stageFlags      = descriptor.stage;
                entry.descriptorCount = 1;

                auto &table = nameTable[set];
                if (!table) {
                    table = std::make_shared<ProperTableInfo>();
                }

                if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
                    auto &type       = compiler.get_type(res.base_type_id);
                    entry.size       = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
                    size_t memberNum = type.member_types.size();
                    for (uint32_t i = 0; i < memberNum; ++i) {
                        uint32_t    offset  = compiler.get_member_decoration(res.base_type_id, i, spv::DecorationOffset);
                        std::string nameKey = name + "." + compiler.get_member_name(res.base_type_id, i);
                        table->handleMap.emplace(nameKey, PropertyHandler{binding, offset});
                    }
                } else {
                    table->handleMap.emplace(name, PropertyHandler{binding, 0});
                }
            }
        };

        auto resources = compiler.get_shader_resources();

        fn(resources.uniform_buffers, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
        fn(resources.storage_buffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        fn(resources.sampled_images, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        fn(resources.storage_images, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        // There can only be one push constant per stage.
        if (!resources.push_constant_buffers.empty()) {
            auto &res         = resources.push_constant_buffers.front();
            auto &type        = compiler.get_type(res.base_type_id);
            constantBlockSize = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
        }

        if (descriptor.stage == VK_SHADER_STAGE_VERTEX_BIT) {
            auto fn = [&compiler, this](const SpvResources &resources) {
                for (auto &res : resources) {
                    auto  location = compiler.get_decoration(res.id, spv::DecorationLocation);
                    auto &name     = compiler.get_name(res.id);
                    auto &info     = stageInputs[name];
                    info.location  = location;
                    auto &type     = compiler.get_type(res.type_id);
                    if (type.basetype == spirv_cross::SPIRType::Float) {
                        if (type.vecsize == 4) {
                            info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                        }
                        if (type.vecsize == 3) {
                            info.format = VK_FORMAT_R32G32B32_SFLOAT;
                        }
                        if (type.vecsize == 2) {
                            info.format = VK_FORMAT_R32G32_SFLOAT;
                        }
                    }
                }
            };
            fn(resources.stage_inputs);
        }
    }

    std::shared_ptr<Shader> Shader::CreateFromData(const ShaderAssetData &data)
    {
        Shader::Descriptor shaderDesc = {};
        shaderDesc.stage = data.stage;
        auto shader = std::make_shared<Shader>(shaderDesc);
        shader->SetData(data.spv);
        shader->InitRHI();
        return shader;
    }

    void ShaderTable::InitRHI()
    {
        if (pipelineLayout) {
            return;
        }

        vk::PipelineLayout::VkDescriptor desc = {};

        VkPushConstantRange range{};
        // merge shader resources.
        for (auto &shader : shaders) {
            auto &table = shader->GetDescriptorTable();
            for (auto &item : table) {
                if (item.first >= desc.desLayouts.size()) {
                    desc.desLayouts.resize(item.first + 1);
                }
                auto &entry = desc.desLayouts[item.first];
                for (auto &binding : item.second.bindings) {
                    auto &merge = entry.bindings[binding.first];
                    merge.stageFlags |= binding.second.stageFlags;
                    merge.descriptorType  = binding.second.descriptorType;
                    merge.descriptorCount = binding.second.descriptorCount;
                    merge.size            = binding.second.size;
                }
            }

            uint32_t pushConstantBlockSize = shader->GetConstantBlockSize();
            if (pushConstantBlockSize != 0) {
                range.stageFlags |= shader->GetShader()->GetShaderStage();
                range.offset = 0;
                range.size   = shader->GetConstantBlockSize();
                desc.pushConstants.emplace_back(range);
            }

            auto &shaderNameTable = shader->GetNameTable();
            for (auto &pair : shaderNameTable) {
                for (auto &[name, handler] : pair.second->handleMap) {
                    auto &handleMap = nameTable[pair.first];
                    if (!handleMap) {
                        handleMap = std::make_shared<ProperTableInfo>();
                    }

                    nameTable[pair.first]->handleMap.emplace(name, handler);
                }
            }
        }
        auto device    = RHIManager::Get()->GetDevice();
        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(desc);
    }

    vk::PipelineLayoutPtr ShaderTable::GetPipelineLayout() const
    {
        return pipelineLayout;
    }

    RDDesGroupPtr ShaderTable::CreateDescriptorGroup(uint32_t slot) const
    {
        auto iter = nameTable.find(slot);
        if (iter == nameTable.end()) {
            return {};
        }

        vk::DescriptorSetLayoutPtr layout = pipelineLayout->GetLayout(slot);
        if (!layout) {
            return {};
        }

        RDDescriptorPoolPtr pool = GlobalDescriptorPool::Get()->GetPool(layout);
        RDDesGroupPtr       res  = pool->Allocate();
        res->SetPropertyTable(iter->second);
        return res;
    }

    void ShaderTable::FillProgram(vk::GraphicsPipeline::Program &program)
    {
        for (auto &shader : shaders) {
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

    void GraphicsShaderTable::SetVS(const RDShaderPtr &vs_)
    {
        vs = vs_;
        shaders.emplace_back(vs);
    }

    void GraphicsShaderTable::SetFS(const RDShaderPtr &fs_)
    {
        fs = fs_;
        shaders.emplace_back(fs);
    }

    bool GraphicsShaderTable::IsValid() const
    {
        return vs && fs;
    }
} // namespace sky
