//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Shader.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <shader/ShaderCompiler.h>

#include <sstream>

#include <core/template/Overloaded.h>
#include <core/hash/Hash.h>
#include <core/hash/Fnv1a.h>

namespace sky {

    struct ShaderCacheHeader {
        uint32_t magic = 0xFE00;
        uint32_t version = 0;
        uint32_t dataSize = 0;
    };

    static rhi::DescriptorType ReplaceDynamic(rhi::DescriptorType type)
    {
        if (type == rhi::DescriptorType::UNIFORM_BUFFER) { return rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC;}
        if (type == rhi::DescriptorType::STORAGE_BUFFER) { return rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC;}
        return type;
    }

    void ShaderPreprocessor::SetValue(const std::string &key, const ValueType &val)
    {
        values[key] = val;
        CalculateHash();
    }

    void ShaderPreprocessor::CalculateHash()
    {
        hash = 0;
        for (auto &[key, val] : values) {
            std::visit(Overloaded{
                           [&](const std::string &v) {
                               HashCombine32(hash, Fnv1a32(v));
                           },
                           [&](const uint32_t &v) {
                               HashCombine32(hash, v);
                           },
                           [&](const auto &){
                               HashCombine32(hash, 1U);
                           }
            }, val);
        }
    }

    std::string ShaderPreprocessor::BuildSource() const
    {
        std::stringstream ss;
        for (const auto& iter : values) {
            std::visit(Overloaded{
                           [&](const Def &v) {
                               ss << "#define " << iter.first << "\n";
                           },
                           [&](const auto &v){
                               ss << "#define " << iter.first << " " << v << "\n";
                           }
                       }, iter.second);
        }
        return ss.str();
    }

    std::string ShaderCollection::RequestSource(const ShaderPreprocessor &p)
    {
        return source + p.BuildSource();
    }

    void Program::SetShader(const ShaderCollectionPtr &shader_)
    {
        shader = shader_;
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

} // name