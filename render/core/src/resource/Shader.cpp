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

                    result.data.resize(shaderHeader.dataSize / sizeof(uint32_t));
                    archive.Load(reinterpret_cast<char*>(result.data.data()), shaderHeader.dataSize);
                }
            }
        }

        if (needUpdateShader) {
            ShaderSourceDesc sourceDesc = {};
            sourceDesc.source = source;
            sourceDesc.entry = entry;
            sourceDesc.stage = stage;

            ShaderCompiler::Get()->Compile(sourceDesc, option, result);

            shaderHeader.version = hash;
            shaderHeader.magic = ShaderCacheHeader::MAGIC;
            shaderHeader.dataSize = static_cast<uint32_t>(result.data.size() * sizeof (uint32_t));

            OFileArchive archive(cachePath);
            archive.Save(reinterpret_cast<const char*>(&shaderHeader), sizeof(ShaderCacheHeader));
            archive.Save(reinterpret_cast<const char*>(result.data.data()), shaderHeader.dataSize);
        }

    }
}