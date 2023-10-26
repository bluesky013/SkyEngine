//
// Created by Zach Lee on 2023/2/23.
//


#include <builder/render/ShaderBuilder.h>
#include <builder/shader/ShaderCompiler.h>
#include <spirv_cross/spirv_glsl.hpp>
#include <framework/asset/AssetManager.h>

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
        ShaderType type;
        rhi::ShaderStageFlagBit stage;
        std::string outExt;
        if (request.ext == ".vert") {
            type = ShaderType::VS;
            stage = rhi::ShaderStageFlagBit::VS;
        } else if (request.ext == ".frag") {
            type = ShaderType::FS;
            stage = rhi::ShaderStageFlagBit::FS;
        } else if (request.ext == ".comp") {
            type = ShaderType::CS;
            stage = rhi::ShaderStageFlagBit::CS;
        } else {
            return;
        }
        auto *am = AssetManager::Get();
        std::filesystem::path outDir = request.outDir + "/shaders";
        std::filesystem::create_directories(outDir);
        std::string fullPath = outDir.append(request.name).make_preferred().string();
        std::string outFullPath = fullPath + ".bin";
        std::string outVariantPath = fullPath + ".variant";
        auto asset = am->CreateAsset<Shader>(outFullPath);
        auto defaultVariant = am->CreateAsset<ShaderVariant>(outVariantPath);

        auto &variantData = defaultVariant->Data();
        // save spv
        ShaderCompiler::BuildSpirV(request.fullPath, type, variantData.spv);
        if (variantData.spv.empty()) {
            return;
        }
        BuildReflection(variantData.spv, asset->Data());

        // save gles
        variantData.gles = ShaderCompiler::BuildGLES(variantData.spv);
        asset->Data().stage = stage;
        asset->Data().variants.emplace("", defaultVariant);

        result.products.emplace_back(BuildProduct{"GFX_SHADER", asset->GetUuid()});
        am->SaveAsset(asset);
        am->SaveAsset(defaultVariant);
    }
}
