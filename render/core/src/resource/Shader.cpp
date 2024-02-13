//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Shader.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <shader/ShaderCompiler.h>

#include <sstream>
#include <filesystem>
#include <ranges>

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

    //    void Program::BuildPipelineLayout()
    //    {
    //        rhi::PipelineLayout::Descriptor plDesc = {};
    //        std::vector<rhi::DescriptorSetLayout::Descriptor> layoutDesc;
    //        for (auto &shader : shaders) {
    //            const auto &resources = shader->GetShaderResources();
    //            for (auto &res : resources) {
    //                if (res.set >= layoutDesc.size()) {
    //                    layoutDesc.resize(res.set + 1);
    //                }
    //
    //                auto &bindings = layoutDesc[res.set].bindings;
    //                auto iter = std::find_if(bindings.begin(), bindings.end(), [&res](const auto &bd) {
    //                    return res.binding == bd.binding;
    //                });
    //                if (iter != bindings.end()) {
    //                    iter->visibility |= shader->GetStage();
    //                } else {
    //                    rhi::DescriptorSetLayout::SetBinding binding = {};
    //                    binding.name = res.name;
    //                    binding.type = res.set >= 1 ? ReplaceDynamic(res.type) : res.type;
    //                    binding.count = 1;
    //                    binding.binding = res.binding;
    //                    binding.visibility = shader->GetStage();
    //                    bindings.emplace_back(binding);
    //                }
    //            }
    //            const auto &constant = shader->GetConstant();
    //            if (constant.size != 0) {
    //                auto &push = plDesc.constants.emplace_back();
    //                push.stageFlags = shader->GetStage();
    //                push.offset = 0;
    //                push.size = constant.size;
    //            }
    //        }
    //
    //        auto *device = RHI::Get()->GetDevice();
    //        for (auto &desc : layoutDesc) {
    //            if (desc.bindings.empty()) {
    //                plDesc.layouts.emplace_back(Renderer::Get()->GetDefaultRHIResource().emptyDesLayout->GetRHILayout());
    //            } else {
    //                plDesc.layouts.emplace_back(device->CreateDescriptorSetLayout(desc));
    //            }
    //        }
    //        pipelineLayout = device->CreatePipelineLayout(plDesc);
    //    }

    RDResourceLayoutPtr Program::RequestLayout(uint32_t index) const
    {
        auto rhiLayout = pipelineLayout->GetSetLayout(index);
        if (!rhiLayout) {
            return {};
        }
        auto layout = std::make_shared<ResourceGroupLayout>();
        layout->SetRHILayout(rhiLayout);

        //        for (const auto &shader : shaders) {
        //            const auto &shaderResources = shader->GetShaderResources();
        //            for (const auto &shaderResource : shaderResources) {
        //                if (shaderResource.set != index) {
        //                    continue;
        //                }
        //
        //                layout->AddNameHandler(shaderResource.name, {shaderResource.binding, shaderResource.size});
        //                for (const auto &member : shaderResource.members) {
        //                    layout->AddBufferNameHandler(member.name, {shaderResource.binding, member.offset, member.size});
        //                }
        //            }
        //        }
        return layout;
    }

    void ShaderCollection::BuildAndCacheShader(const std::string &entry, rhi::ShaderStageFlagBit stage, const ShaderCompileOption &option, ShaderBuildResult &result)
    {
        // calculate signature
        std::stringstream ss;
        uint32_t variantHash = option.preprocessor ? option.preprocessor->GetHash() : 0;
        ss << name << '_' << entry << '_' << variantHash << '_' << static_cast<uint32_t>(option.target);

        auto cacheFolder = Renderer::Get()->GetCacheFolder() + "/shaderCache";
        std::string cachePath = cacheFolder + "/" + ss.str();

        if (!std::filesystem::exists(cacheFolder)) {
            std::filesystem::create_directories(cacheFolder);
        }

        bool needUpdateShader = true;
        ShaderCacheHeader shaderHeader = {};
        {
            IFileArchive archive(cachePath);
            if (archive.IsOpen()) {
                archive.Load(reinterpret_cast<char*>(&shaderHeader), sizeof(ShaderCacheHeader));

                if (shaderHeader.magic == ShaderCacheHeader::MAGIC && shaderHeader.version == hash) {
                    needUpdateShader = false;
                    archive.Load(result.data);
                    uint32_t size = 0;
                    archive.Load(size);
                    result.reflection.resources.resize(size);
                    for (auto i : std::views::iota(0U, size)) {
                        auto &res = result.reflection.resources[i];

                        archive.Load(res.name);
                        archive.Load(res.type);
                        archive.Load(res.visibility.value);
                        archive.Load(res.group);
                        archive.Load(res.binding);
                        archive.Load(res.count);
                        archive.Load(res.size);
                    }
                    archive.Load(size);
                    result.reflection.variables.resize(size);
                    for (auto i : std::views::iota(0U, size)) {
                        auto &var = result.reflection.variables[i];
                        archive.Load(var.name);
                        archive.Load(var.group);
                        archive.Load(var.binding);
                        archive.Load(var.offset);
                        archive.Load(var.size);
                    }
                }
            }
        }
        if (needUpdateShader) {
            ShaderSourceDesc sourceDesc = {};
            sourceDesc.source = source;
            sourceDesc.entry = entry;
            sourceDesc.stage = stage;

            ShaderCompiler::Get()->Compile(sourceDesc, option, result);

            MemoryArchive mArchive;
            mArchive.Save(result.data);

            mArchive.Save(static_cast<uint32_t>(result.reflection.resources.size()));
            for (auto &res : result.reflection.resources) {
                mArchive.Save(res.name);
                mArchive.Save(res.type);
                mArchive.Save(res.visibility.value);
                mArchive.Save(res.group);
                mArchive.Save(res.binding);
                mArchive.Save(res.count);
                mArchive.Save(res.size);
            }

            mArchive.Save(static_cast<uint32_t>(result.reflection.variables.size()));
            for (auto &var : result.reflection.variables) {
                mArchive.Save(var.name);
                mArchive.Save(var.group);
                mArchive.Save(var.binding);
                mArchive.Save(var.offset);
                mArchive.Save(var.size);
            }

            shaderHeader.version = hash;
            shaderHeader.magic = ShaderCacheHeader::MAGIC;
            shaderHeader.dataSize = static_cast<uint32_t>(mArchive.GetData().size());

            OFileArchive archive(cachePath);
            archive.Save(reinterpret_cast<const char*>(&shaderHeader), sizeof(ShaderCacheHeader));
            archive.Save(reinterpret_cast<const char*>(mArchive.GetData().data()), shaderHeader.dataSize);
        }

    }
}