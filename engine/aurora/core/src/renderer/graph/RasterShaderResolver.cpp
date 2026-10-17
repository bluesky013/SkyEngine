//
// Created on 2026/10/16.
//

#include "graph/RasterShaderResolver.h"

#include <algorithm>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/Device.h>
#include <aurora/shader/ShaderBuild.h>

namespace sky::aurora {
    std::variant<FrameError, ShaderPtr> ResolveRasterShader(Device &device, ShaderResolver &resolver, const RasterShaderRequest &request)
    {
        if (device.GetAPI() != API::VULKAN && device.GetAPI() != API::DX12)
            return FrameError{FrameErrorCode::UNSUPPORTED};
        const auto target = static_cast<uint32_t>(device.GetAPI() == API::VULKAN ? ShaderTarget::SPIRV : ShaderTarget::DXIL);
        if (request.vertex.stage != ShaderStageFlagBit::VS || request.fragment.stage != ShaderStageFlagBit::FS || request.vertex.target != target ||
            request.fragment.target != target || request.vertex.entry.empty() || request.fragment.entry.empty() ||
            request.dynamicUniforms.size() > 32)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid raster shader stages/target"};
        ShaderCompileResult vertex, fragment;
        std::string         message;
        if (!resolver.ResolveShader(request.vertex, vertex, &message) || !resolver.ResolveShader(request.fragment, fragment, &message))
            return FrameError{FrameErrorCode::COMPILE_FAILED, std::move(message)};
        ShaderReflection reflection;
        for (const auto *result : {&vertex, &fragment}) {
            if (result->data.empty() || result->data.size() > UINT32_MAX / sizeof(uint32_t))
                return FrameError{FrameErrorCode::COMPILE_FAILED, "Invalid shader bytecode size"};
            if (!result->reflection.pushConstants.empty())
                return FrameError{FrameErrorCode::UNSUPPORTED, "Raster push constants are not implemented"};
            for (const auto &resource : result->reflection.resources) {
                if (!IsRasterResourceSupported(resource))
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Raster shader requires scalar UBO, float4 Texture2D/Array or regular sampler"};
                auto found = std::find_if(reflection.resources.begin(), reflection.resources.end(),
                                          [&](const auto &r) { return r.set == resource.set && r.binding == resource.binding; });
                if (found == reflection.resources.end())
                    reflection.resources.push_back(resource);
                else if (found->type != resource.type || found->count != resource.count || found->descriptorArray != resource.descriptorArray ||
                         found->texture != resource.texture || found->sampler != resource.sampler)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Incompatible stage resource layouts"};
            }
            for (const auto &block : result->reflection.blocks) {
                auto found = std::find_if(reflection.blocks.begin(), reflection.blocks.end(),
                                          [&](const auto &b) { return b.set == block.set && b.binding == block.binding; });
                if (found == reflection.blocks.end())
                    reflection.blocks.push_back(block);
                else if (found->size != block.size || found->structName != block.structName || found->members.size() != block.members.size())
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Incompatible stage uniform blocks"};
                else
                    for (size_t i = 0; i < block.members.size(); ++i) {
                        const auto &a = block.members[i], &b = found->members[i];
                        if (a.offset != b.offset || a.size != b.size || a.scalarType != b.scalarType || a.kind != b.kind || a.rows != b.rows ||
                            a.cols != b.cols)
                            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Incompatible stage uniform fields"};
                    }
            }
        }
        for (size_t i = 0; i < request.dynamicUniforms.size(); ++i) {
            const auto &binding = request.dynamicUniforms[i];
            auto        found   = std::find_if(reflection.resources.begin(), reflection.resources.end(),
                                               [&](const auto &r) { return r.set == binding.set && r.binding == binding.binding; });
            if (found == reflection.resources.end() || found->type != ShaderResourceType::UNIFORM_BUFFER)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing or duplicate dynamic UBO binding"};
            found->type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        }
        const auto vs = CreateShaderFunctionFromResult(device, vertex, ShaderStageFlagBit::VS, request.vertex.entry);
        const auto fs = CreateShaderFunctionFromResult(device, fragment, ShaderStageFlagBit::FS, request.fragment.entry);
        if (!vs || !fs)
            return FrameError{FrameErrorCode::COMPILE_FAILED, "Native shader function creation failed"};
        Shader::Descriptor descriptor{};
        descriptor.vs         = vs.Get();
        descriptor.ps         = fs.Get();
        descriptor.reflection = &reflection;
        ShaderPtr shader(device.CreateShader(descriptor));
        if (!shader)
            return FrameError{FrameErrorCode::COMPILE_FAILED, "Native raster shader creation failed"};
        return shader;
    }
} // namespace sky::aurora
