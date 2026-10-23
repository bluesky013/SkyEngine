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
        if (device.GetAPI() != API::VULKAN && device.GetAPI() != API::DX12 && device.GetAPI() != API::METAL)
            return FrameError{FrameErrorCode::UNSUPPORTED};
        const auto target = static_cast<uint32_t>(device.GetAPI() == API::VULKAN  ? ShaderTarget::SPIRV
                                                  : device.GetAPI() == API::METAL ? ShaderTarget::MSL
                                                                                  : ShaderTarget::DXIL);
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
            if (!result->reflection.pushConstants.empty())
                return FrameError{FrameErrorCode::UNSUPPORTED, "Raster push constants are not implemented"};
            for (const auto &resource : result->reflection.resources)
                if (!IsRasterResourceSupported(resource))
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported raster resource"};
            if (!MergeShaderReflections(result->reflection, reflection, message))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, std::move(message)};
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
        if (!vs || !fs || vs->GetDevice() != &device || fs->GetDevice() != &device)
            return FrameError{FrameErrorCode::COMPILE_FAILED, "Native shader function creation failed"};
        Shader::Descriptor descriptor{};
        descriptor.vs         = vs.Get();
        descriptor.ps         = fs.Get();
        descriptor.reflection = &reflection;
        for (const auto &binding : request.dynamicUniforms)
            descriptor.bindingPolicies.push_back({binding.set, binding.binding, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC});
        ShaderPtr shader(device.CreateShader(descriptor));
        if (!shader || shader->GetDevice() != &device)
            return FrameError{FrameErrorCode::COMPILE_FAILED, "Native raster shader creation failed"};
        return shader;
    }
} // namespace sky::aurora
