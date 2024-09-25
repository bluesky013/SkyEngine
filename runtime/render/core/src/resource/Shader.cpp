//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Shader.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <shader/ShaderCompiler.h>

#include <sstream>
#include <filesystem>

#include <core/template/Overloaded.h>
#include <core/hash/Hash.h>
#include <core/hash/Fnv1a.h>
#include <core/archive/FileArchive.h>
#include <core/archive/MemoryArchive.h>

namespace sky {

    struct ShaderCacheHeader {
        static const uint32_t MAGIC = 0xFE00;

        uint32_t magic    = MAGIC;
        uint32_t version  = 0;
        uint32_t dataSize = 0;
    };

    static rhi::DescriptorType ReplaceDynamic(rhi::DescriptorType type)
    {
        if (type == rhi::DescriptorType::UNIFORM_BUFFER) {
            return rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC;
        }
        if (type == rhi::DescriptorType::STORAGE_BUFFER) {
            return rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC;
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
                plDesc.layouts.emplace_back(Renderer::Get()->GetDefaultRHIResource().emptyDesLayout->GetRHILayout());
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

            layout->AddNameHandler(res.name, {res.binding, res.size});
            if (res.type == rhi::DescriptorType::UNIFORM_BUFFER || res.type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
                auto iter = std::find_if(reflection->types.begin(), reflection->types.end(), [&res](const auto &type) -> bool {
                    return res.name == type.name;
                });
                SKY_ASSERT(iter != reflection->types.end());

                for (const auto &var : iter->variables) {
                    layout->AddBufferNameHandler(var.name, {var.binding, var.offset, var.size});
                }
            }
        }
        return layout;
    }

    void ShaderCollection::BuildAndCacheShader(const std::string         &entry,
                                               rhi::ShaderStageFlagBit    stage,
                                               const ShaderCompileOption &option,
                                               ShaderBuildResult         &result)
    {
        // calculate signature
        std::stringstream ss;
        uint32_t          variantHash = option.option ? option.option->GetHash() : 0;
        ss << name << '_' << entry << '_' << variantHash << '_' << static_cast<uint32_t>(option.target);

        auto        cacheFolder = Renderer::Get()->GetCacheFolder() + "\\shaderCache";
        std::string cachePath   = cacheFolder + "\\" + ss.str();

        if (!std::filesystem::exists(cacheFolder)) {
            std::filesystem::create_directories(cacheFolder);
        }

        bool              needUpdateShader = true;
        ShaderCacheHeader shaderHeader     = {};
        {
            IFileArchive archive(cachePath);
            if (archive.IsOpen()) {
                archive.LoadRaw(reinterpret_cast<char *>(&shaderHeader), sizeof(ShaderCacheHeader));

                if (shaderHeader.magic == ShaderCacheHeader::MAGIC && shaderHeader.version == hash) {
                    needUpdateShader = false;
                    archive.Load(result.data);
                    uint32_t size = 0;
                    archive.Load(size);
                    result.reflection.resources.resize(size);
                    for (auto i = 0U; i < size; ++i) {
                        auto &res = result.reflection.resources[i];

                        archive.Load(res.name);
                        archive.Load(res.type);
                        archive.Load(res.visibility.value);
                        archive.Load(res.set);
                        archive.Load(res.binding);
                        archive.Load(res.count);
                        archive.Load(res.size);
                    }
                    archive.Load(size);
                    result.reflection.types.resize(size);
                    for (auto i = 0U; i < size; ++i) {
                        auto &type = result.reflection.types[i];
                        archive.Load(type.name);
                        uint32_t varSize = 0;
                        archive.Load(varSize);
                        type.variables.resize(varSize);
                        for (auto j = 0U; j < varSize; ++j) {
                            auto &var = type.variables[j];
                            archive.Load(var.name);
                            archive.Load(var.set);
                            archive.Load(var.binding);
                            archive.Load(var.offset);
                            archive.Load(var.size);
                        }
                    }
                }
            }
        }
        if (needUpdateShader) {
            ShaderSourceDesc sourceDesc = {};
            sourceDesc.source           = source;
            sourceDesc.entry            = entry;
            sourceDesc.stage            = stage;

            auto *compiler = Renderer::Get()->GetShaderCompiler();
            if (compiler == nullptr || !compiler(sourceDesc, option, result)) {
                return;
            }

//            MemoryArchive mArchive;
//            mArchive.Save(result.data);
//
//            mArchive.Save(static_cast<uint32_t>(result.reflection.resources.size()));
//            for (auto &res : result.reflection.resources) {
//                mArchive.Save(res.name);
//                mArchive.Save(res.type);
//                mArchive.Save(res.visibility.value);
//                mArchive.Save(res.set);
//                mArchive.Save(res.binding);
//                mArchive.Save(res.count);
//                mArchive.Save(res.size);
//            }
//
//            mArchive.Save(static_cast<uint32_t>(result.reflection.types.size()));
//            for (auto &type : result.reflection.types) {
//                mArchive.Save(type.name);
//                mArchive.Save(static_cast<uint32_t>(type.variables.size()));
//                for (auto &var : type.variables) {
//                    mArchive.Save(var.name);
//                    mArchive.Save(var.set);
//                    mArchive.Save(var.binding);
//                    mArchive.Save(var.offset);
//                    mArchive.Save(var.size);
//                }
//            }
//
//            shaderHeader.version  = hash;
//            shaderHeader.magic    = ShaderCacheHeader::MAGIC;
//            shaderHeader.dataSize = static_cast<uint32_t>(mArchive.Size());
//
//            OFileArchive archive(cachePath);
//            archive.SaveRaw(reinterpret_cast<const char *>(&shaderHeader), sizeof(ShaderCacheHeader));
//            archive.SaveRaw(reinterpret_cast<const char *>(mArchive.Data()), shaderHeader.dataSize);
        }
    }
} // namespace sky