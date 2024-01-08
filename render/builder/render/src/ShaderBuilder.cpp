//
// Created by Zach Lee on 2023/2/23.
//

#include <builder/render/ShaderBuilder.h>
#include <builder/shader/ShaderCompiler.h>
#include <framework/asset/AssetManager.h>
#include <spirv_cross/spirv_glsl.hpp>

#include <filesystem>
#include <render/adaptor/assets/ShaderAsset.h>

namespace sky::builder {

    void BuildReflection(const std::vector<uint32_t> &spv, ShaderAssetData &data)
    {
        spirv_cross::CompilerGLSL compiler(spv.data(), spv.size());
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        auto visitor = [&compiler, &data](auto &resources, rhi::DescriptorType type) {
            for (auto &resource : resources) {
                const auto &spvType = compiler.get_type(resource.base_type_id);

                data.resources.emplace_back();
                auto &descriptor    = data.resources.back();
                descriptor.type     = type;
                descriptor.set      = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                descriptor.binding  = compiler.get_decoration(resource.id, spv::DecorationBinding);
                descriptor.name     = compiler.get_name(resource.id);
                if (descriptor.name.empty()) {
                    descriptor.name     = compiler.get_name(resource.base_type_id);
                }
                if (type == rhi::DescriptorType::UNIFORM_BUFFER || type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
                    descriptor.size = static_cast<uint32_t>(compiler.get_declared_struct_size(spvType));
                }

                for (uint32_t index = 0; index < spvType.member_types.size(); ++index) {
                    const auto  memberOffset = compiler.type_struct_member_offset(spvType, index);
                    const auto  memberSize   = compiler.get_declared_struct_member_size(spvType, index);
                    const auto &memberName   = compiler.get_member_name(resource.base_type_id, index);

                    descriptor.members.emplace_back(
                        ShaderBufferMember{memberName, memberOffset, static_cast<uint32_t>(memberSize)});
                }
            }
        };

        visitor(resources.sampled_images, rhi::DescriptorType::COMBINED_IMAGE_SAMPLER);
        visitor(resources.storage_images, rhi::DescriptorType::STORAGE_IMAGE);
        visitor(resources.uniform_buffers, rhi::DescriptorType::UNIFORM_BUFFER);
        visitor(resources.storage_buffers, rhi::DescriptorType::STORAGE_BUFFER);
        visitor(resources.subpass_inputs, rhi::DescriptorType::INPUT_ATTACHMENT);

        for (auto &resource : resources.push_constant_buffers) {
            const auto &spvType = compiler.get_type(resource.base_type_id);
            data.pushConstant.size = static_cast<uint32_t>(compiler.get_declared_struct_size(spvType));

            for (uint32_t index = 0; index < spvType.member_types.size(); ++index) {
                const auto  memberOffset = compiler.type_struct_member_offset(spvType, index);
                const auto  memberSize   = compiler.get_declared_struct_member_size(spvType, index);
                const auto &memberName   = compiler.get_member_name(resource.base_type_id, index);

                data.pushConstant.members.push_back(ShaderBufferMember{memberName, memberOffset, static_cast<uint32_t>(memberSize)});
            }
        }
    }

    void ShaderBuilder::Request(const BuildRequest &request, BuildResult &result)
    {
        if (!compiler) {
            compiler = std::make_unique<sl::ShaderCompiler>();
            for (const auto &path : AssetManager::Get()->GetSearchPathList()) {
                compiler->AddEngineIncludePath(path.path);
            }
        }

        sl::ShaderType          type;
        rhi::ShaderStageFlagBit stage;
        std::string             outExt;
        if (request.ext == ".vert") {
            type  = sl::ShaderType::VS;
            stage = rhi::ShaderStageFlagBit::VS;
        } else if (request.ext == ".frag") {
            type  = sl::ShaderType::FS;
            stage = rhi::ShaderStageFlagBit::FS;
        } else if (request.ext == ".comp") {
            type  = sl::ShaderType::CS;
            stage = rhi::ShaderStageFlagBit::CS;
        } else {
            return;
        }
        auto *am  = AssetManager::Get();
        auto asset = am->CreateAsset<Shader>(request.uuid);

        // save spv
        sl::ShaderCompiler::Option option = {};

        option.language = sl::Language::GLSL;
        option.type     = type;

        std::vector<uint32_t> spv;
        compiler->BuildSpirV(request.fullPath, spv, option);
        if (spv.empty()) {
            return;
        }
        BuildReflection(spv, asset->Data());

        result.products.emplace_back(BuildProduct{"GFX_SHADER", asset});
        result.success = true;
//
//        // save gles
//        //        variantData.gles = ShaderCompiler::BuildGLES(variantData.spv);
//        asset->Data().stage = stage;
//        asset->Data().variants.emplace("", defaultVariant);
//
//        result.products.emplace_back(BuildProduct{"GFX_SHADER", asset->GetUuid()});
//        am->SaveAsset(asset);
//        am->SaveAsset(defaultVariant);
    }

    void ShaderBuilder::LoadConfig(const std::string &path)
    {
    }
}
