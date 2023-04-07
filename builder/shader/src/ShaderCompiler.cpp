//
// Created by Zach Lee on 2023/2/18.
//

#include "builder/shader/ShaderCompiler.h"

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cpp.hpp>

#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <framework/asset/AssetManager.h>

#include <filesystem>

static const char* TAG = "ShaderCompiler";

namespace sky::builder {
    using SpvResources = spirv_cross::SmallVector<spirv_cross::Resource>;
    static std::string GetFullPath(const std::string &relative)
    {
        std::filesystem::path rPath(relative);
        if (rPath.is_absolute()) {
            return relative;
        }

        const auto &pathList = AssetManager::Get()->GetSearchPaths();
        for (const auto &path : pathList) {
            std::filesystem::path fPath(path);
            fPath.append(relative);
            if (!std::filesystem::exists(fPath)) {
                continue;
            }
            return fPath.string();
        }
        return "";
    }

    static void SaveSpv(const std::string &path, const std::vector<uint32_t> &spv)
    {
        WriteBin(path, reinterpret_cast<const char *>(spv.data()), spv.size() * sizeof(uint32_t));
    }

    static void SaveGLES(const std::string &path, const std::string &source)
    {
        WriteString(path, source);
    }

    static shaderc_shader_kind GetShaderKind(ShaderType type)
    {
        switch (type) {
        case ShaderType::VS: return shaderc_vertex_shader;
        case ShaderType::FS: return shaderc_fragment_shader;
        case ShaderType::CS: return shaderc_compute_shader;
        }
        return shaderc_vertex_shader;
    }

    class IncluderImpl : public shaderc::CompileOptions::IncluderInterface {
    public:
        using ShaderResultPtr = std::unique_ptr<shaderc_include_result>;
        struct Data {
            ShaderResultPtr ptr;
            std::string source;
            std::string content;
        };

        shaderc_include_result *
        GetInclude(const char *requested_source, shaderc_include_type type, const char *requesting_source, size_t include_depth) override
        {
            std::string requestHeader = GetFullPath(requested_source);
            auto iter = sourceMap.find(requestHeader);
            if (iter != sourceMap.end()) {
                return iter->second.ptr.get();
            }

            std::string source;
            ReadString(requestHeader, source);
            auto &result = sourceMap.emplace(requestHeader, Data{std::make_unique<shaderc_include_result>(), requestHeader, source}).first->second;

            result.ptr->source_name = result.source.c_str();
            result.ptr->source_name_length = result.source.length();
            result.ptr->content = result.content.c_str();
            result.ptr->content_length = result.content.length();
            result.ptr->user_data = nullptr;
            return result.ptr.get();
        }

        void ReleaseInclude(shaderc_include_result *data) override
        {
        }


    private:

        std::unordered_map<std::string, Data> sourceMap;
    };

    void ShaderCompiler::BuildSpirV(const std::string &path, ShaderType type, std::vector<uint32_t> &out)
    {
        shaderc::Compiler       compiler;
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(shaderc_target_env_vulkan, 0);
        options.SetIncluder(std::unique_ptr<shaderc::CompileOptions::IncluderInterface>(new IncluderImpl()));
        options.SetInvertY(true); // hlsl only

        auto realPath = GetFullPath(path);
        if (realPath.empty()) {
            return;
        }

        std::string source;
        ReadString(realPath, source);
        auto result = compiler.CompileGlslToSpv(source, GetShaderKind(type), realPath.c_str(), options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            LOG_E(TAG, "compile result, shader %s, result %d\n, error info: %s", realPath.c_str(), result.GetCompilationStatus(),
                  result.GetErrorMessage().c_str());
            return;
        }
        out.assign(result.begin(), result.end());
    }

    std::string ShaderCompiler::BuildGLES(const std::vector<uint32_t> &spv)
    {
        spirv_cross::CompilerGLSL compiler(spv.data(), spv.size());
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        auto remap = [&compiler](auto &resources) {
            for (auto &resource : resources) {
                unsigned set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
                unsigned binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

                // Modify the decoration to prepare it for GLSL.
                compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);

                // Some arbitrary remapping if we want.
                compiler.set_decoration(resource.id, spv::DecorationBinding, set * 8 + binding);
            }
        };

        remap(resources.uniform_buffers);
        remap(resources.storage_buffers);
        remap(resources.sampled_images);
        remap(resources.storage_images);


        spirv_cross::CompilerGLSL::Options options;
        options.version = 320;
        options.es = true;
        options.vertex.flip_vert_y = true;
        compiler.set_common_options(options);
        return compiler.compile();
        //        auto fn            = [&compiler](const SpvResources &resources) {
        //            for (auto &res : resources) {
        //                auto  set     = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
        //                auto  binding = compiler.get_decoration(res.id, spv::DecorationBinding);
        //                auto &name    = compiler.get_name(res.id);
        //
        //                LOG_I(TAG, "shader resource set %u, binding %u, name %s", set, binding, name.c_str());
        //            }
        //        };
        //        auto resources = compiler.get_shader_resources();
        //        fn(resources.uniform_buffers);
        //        fn(resources.storage_buffers);
        //        fn(resources.sampled_images);
        //        fn(resources.storage_images);
        //
        //        std::string src = compiler.compile();
        //        WriteString(path, src);
    }

    void ShaderCompiler::CompileShader(const std::string &path, const Option &option)
    {
        std::vector<uint32_t> spv;
        BuildSpirV(path, option.type, spv);
        SaveSpv(option.output + ".spv", spv);

        std::string glslSrc = BuildGLES(spv);
        SaveGLES(option.output + ".gles", glslSrc);
    }
}