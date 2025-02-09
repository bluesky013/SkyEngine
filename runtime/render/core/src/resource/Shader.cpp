//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Shader.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderFileSystem.h>

#include <sstream>
#include <filesystem>

#include <core/template/Overloaded.h>
#include <core/hash/Hash.h>
#include <core/hash/Fnv1a.h>
#include <core/archive/FileArchive.h>
#include <core/archive/MemoryArchive.h>

namespace sky {

    static rhi::DescriptorType ReplaceDynamic(rhi::DescriptorType type)
    {
        if (type == rhi::DescriptorType::UNIFORM_BUFFER) {
            return rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC;
        }
        return type;
    }

    void Program::MergeReflection(ShaderReflection &&refl)
    {
        if (!reflection) {
            reflection = std::make_unique<ShaderReflection>(std::move(refl));
            return;
        }

        for (auto &resource : refl.resources) {
            auto iter = std::find_if(reflection->resources.begin(), reflection->resources.end(),
                [&resource](const ShaderResource &res) -> bool {
                return resource.name == res.name;
            });
            if (iter != reflection->resources.end()) {
                auto &current = *iter;
                SKY_ASSERT(current.set == resource.set);
                SKY_ASSERT(current.binding == resource.binding);
                SKY_ASSERT(current.type == resource.type);
                current.visibility |= resource.visibility;
            } else {
                reflection->resources.emplace_back(resource);
            }
        }

        for (auto &type : refl.types) {
            auto iter = std::find_if(reflection->types.begin(), reflection->types.end(),
            [&type](const ShaderStructType &val) -> bool {
                return type.name == val.name;
            });
            if (iter == reflection->types.end()) {
                reflection->types.emplace_back(type);
            }
        }
    }

    void Program::Build()
    {
        rhi::PipelineLayout::Descriptor plDesc = {};
        std::vector<rhi::DescriptorSetLayout::Descriptor> layoutDesc;

        for (const auto &res : reflection->resources) {
            if (res.set >= layoutDesc.size()) {
                layoutDesc.resize(res.set + 1);
            }

            auto &bindings = layoutDesc[res.set].bindings;
            rhi::DescriptorSetLayout::SetBinding binding = {};
            binding.name       = res.name;
            binding.type       = res.set >= 1 ? ReplaceDynamic(res.type) : res.type;
            binding.count      = std::max(1U, res.count);
            binding.binding    = res.binding;
            binding.visibility = res.visibility;
            bindings.emplace_back(binding);
        }
//            const auto &constant = shader->GetConstant();
//            if (constant.size != 0) {
//                auto &push      = plDesc.constants.emplace_back();
//                push.stageFlags = shader->GetStage();
//                push.offset     = 0;
//                push.size       = constant.size;
//            }

        auto *device = RHI::Get()->GetDevice();
        for (auto &desc : layoutDesc) {
            if (desc.bindings.empty()) {
                plDesc.layouts.emplace_back(Renderer::Get()->GetDefaultResource().emptyDesLayout->GetRHILayout());
            } else {
                plDesc.layouts.emplace_back(device->CreateDescriptorSetLayout(desc));
            }
        }
        pipelineLayout = device->CreatePipelineLayout(plDesc);
    }

    RDResourceLayoutPtr Program::RequestLayout(uint32_t index) const
    {
        auto rhiLayout = pipelineLayout->GetSetLayout(index);
        if (!rhiLayout) {
            return {};
        }
        auto *layout = new ResourceGroupLayout();
        layout->SetRHILayout(rhiLayout);

        for (const auto &res : reflection->resources) {
            if (res.set != index) {
                continue;
            }

            layout->AddNameHandler(Name(res.name.c_str()), {res.binding, res.size});
            if (res.type == rhi::DescriptorType::UNIFORM_BUFFER || res.type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
                auto iter = std::find_if(reflection->types.begin(), reflection->types.end(), [&res](const auto &type) -> bool {
                    return res.name == type.name;
                });
                SKY_ASSERT(iter != reflection->types.end());

                for (const auto &var : iter->variables) {
                    layout->AddBufferNameHandler(Name(var.name.c_str()), {var.binding, var.offset, var.size});
                }
            }
        }
        return layout;
    }

    ShaderCollection::ShaderCollection(const Name &name_, const ShaderSourceEntry &source)
        : RenderResource(name_)
    {
        for (const auto &option : source.options) {
            list.AddEntry(option);
        }
    }

    void ShaderCollection::CacheProgram(const ShaderVariantKey &key, const RDProgramPtr &program)
    {
        std::lock_guard<std::mutex> lock(mutex);
        programCache[key] = program;
    }

    RDProgramPtr ShaderCollection::FetchProgramCache(const ShaderVariantKey &key) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = programCache.find(key);
        if (iter != programCache.end()) {
            return iter->second;
        }
        return {};
    }

    RDProgramPtr ShaderCollection::AcquireShaderBinary(const sky::ShaderVariantKey &key, const std::vector<std::pair<Name, rhi::ShaderStageFlagBit>> &stages)
    {
        RDProgramPtr program = FetchProgramCache(key);
        if (program) {
            return program;
        }
        program = RDProgramPtr(new Program());

        auto target = RHI::Get()->GetShaderTarget();
        auto *device = RHI::Get()->GetDevice();
        auto *cacheManager = ShaderCacheManager::Get();

        bool useMeshShader = std::find_if(stages.begin(), stages.end(), [](const auto &pair) -> bool {
            return pair.second == rhi::ShaderStageFlagBit::MS;
        }) != stages.end();

        for (const auto &[entry, stage] : stages) {

            ShaderBuildResult result;

            const auto *cache = cacheManager->FetchBinaryCache(name, target, entry, key);
            if (cache != nullptr) {
                auto memory = ShaderFileSystem::Get()->LoadBinaryCache(*cache, target);
                ShaderCompiler::LoadFromMemory(*memory, result);
            } else {
                ShaderCompileOption option = {};
                option.target = RHI::Get()->GetShaderTarget();
                option.option = new ShaderOption();
                option.useMeshShader = useMeshShader;
                list.FillShaderOption(*option.option, key);

                auto [rst, source] = ShaderFileSystem::Get()->LoadCacheSource(name);
                SKY_ASSERT(rst);
                ShaderSourceDesc desc = {};
                desc.source = std::move(source);
                desc.entry = entry.GetStr().data();
                desc.stage = stage;

                if (!BuildShaderBinary(desc, option, result)) {
                    return {};
                }
                cacheManager->SaveBinaryCache(name, target, entry, key, result);
            }

            rhi::Shader::Descriptor shaderDesc = {};
            shaderDesc.data = reinterpret_cast<const uint8_t *>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size()) * sizeof(uint32_t);
            shaderDesc.stage = stage;

            auto shader = device->CreateShader(shaderDesc);
            shader->SetEntry(entry.GetStr().data());
            program->AddShader(shader);
            program->MergeReflection(std::move(result.reflection));

        }
        program->Build();
        CacheProgram(key, program);
        return program;
    }

    bool ShaderCollection::BuildShaderBinary(const ShaderSourceDesc &source, const ShaderCompileOption &option, ShaderBuildResult &result)
    {
        auto *compiler = Renderer::Get()->GetShaderCompiler();
        if (compiler != nullptr) {
            return compiler(source, option, result);
        }

        return false;
    }
} // namespace sky