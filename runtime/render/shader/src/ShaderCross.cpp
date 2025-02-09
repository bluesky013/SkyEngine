//
// Created by blues on 2024/2/18.
//

#include <shader/ShaderCross.h>
#include <spirv_cross/spirv_glsl.hpp>

namespace sky {

    void BuildReflectionSPIRV(rhi::ShaderStageFlagBit stage, ShaderBuildResult &result)
    {
        spirv_cross::CompilerGLSL compiler(result.data.data(), result.data.size());
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        auto activeIds = compiler.get_active_interface_variables();

        auto remap = [&compiler, &result, &activeIds, stage](auto &resources, rhi::DescriptorType type) {
            for (auto &resource : resources) {
                auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                bool isVisible = activeIds.count(resource.id) != 0u;
                if (!isVisible && (set != 0)) {
                    continue;
                }

                auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
                const auto &resType = compiler.get_type(resource.type_id);

                result.reflection.resources.emplace_back();
                auto &res = result.reflection.resources.back();

                rhi::ShaderStageFlags visibility = stage;
                if (stage != rhi::ShaderStageFlagBit::CS && set == 0) {
                    visibility = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
                }

                res.type = type;
                res.binding = binding;
                res.set = set;
                res.visibility = visibility;
                res.name = resource.name;
                res.size = 0;
                res.count = resType.array.empty() ? 0 : resType.array[0];

                static const char* TYPE_PREFIX = "type.";
                size_t pos = res.name.find(TYPE_PREFIX);
                if (pos != std::string::npos) {
                    res.name.erase(pos, strlen(TYPE_PREFIX));
                }

                if (type == rhi::DescriptorType::UNIFORM_BUFFER) {
                    res.size = static_cast<uint32_t>(compiler.get_declared_struct_size(resType));

                    auto iter = std::find_if(result.reflection.types.begin(), result.reflection.types.end(),
                                             [&res](const auto &type) {
                                                 return res.name == type.name;
                                             });

                    if (iter == result.reflection.types.end()) {
                        result.reflection.types.emplace_back();
                        auto &structType = result.reflection.types.back();
                        structType.name = res.name;
                        auto &variables = structType.variables;

                        for (auto i  = 0; i < resType.member_types.size(); ++i) {
                            variables.emplace_back();
                            auto &var   = variables.back();
                            var.set     = set;
                            var.binding = binding;
                            var.size    = static_cast<uint32_t>(compiler.get_declared_struct_member_size(resType, i));
                            var.offset  = compiler.type_struct_member_offset(resType, i);
                            var.name    = compiler.get_member_name(resource.base_type_id, i);
                        }
                    }
                }
            }
        };

        remap(resources.uniform_buffers, rhi::DescriptorType::UNIFORM_BUFFER);
        remap(resources.storage_buffers, rhi::DescriptorType::STORAGE_BUFFER);
        remap(resources.separate_images, rhi::DescriptorType::SAMPLED_IMAGE);
        remap(resources.separate_samplers, rhi::DescriptorType::SAMPLER);
        remap(resources.storage_images, rhi::DescriptorType::STORAGE_IMAGE);
        remap(resources.subpass_inputs, rhi::DescriptorType::INPUT_ATTACHMENT);
    }

} // namespace sky