//
// Created by Zach Lee on 2023/2/18.
//

#include "builder/shader/ShaderCompiler.h"

#include <spirv_cross/spirv_cpp.hpp>
#include <spirv_cross/spirv_msl.hpp>

#include <core/file/FileIO.h>
#include <core/logger/Logger.h>

#include <core/platform/Platform.h>

// filesystem
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <filesystem>
#include <fstream>

static const char* TAG = "ShaderCompiler";

namespace sky::sl {
    static std::pair<bool, std::string> GetShaderSource(const std::string &path)
    {
        std::fstream f(path, std::ios::binary | std::ios::in);
        if (!f.is_open()) {
            return {false, ""};
        }

        std::string str((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        return {true, str};
    }

    static std::size_t ReplaceAll(std::string& inout, std::string_view what, std::string_view with)
    {
        std::size_t count{};
        for (std::string::size_type pos{};
             std::string::npos != (pos = inout.find(what.data(), pos, what.length()));
             pos += with.length(), ++count) {
            inout.replace(pos, what.length(), with.data(), with.length());
        }
        return count;
    }

//    static std::string GetFullPath(const std::string &relative)
//    {
//        std::filesystem::path rPath(relative);
//        if (rPath.is_absolute()) {
//            return relative;
//        }
//
//        const auto &pathList = AssetManager::Get()->GetSearchPaths();
//        for (const auto &path : pathList) {
//            std::filesystem::path fPath(path);
//            fPath.append(relative);
//            if (!std::filesystem::exists(fPath)) {
//                continue;
//            }
//            return fPath.string();
//        }
//        return relative;
//    }

    EShLanguage GetLanguage(ShaderType type)
    {
        switch (type) {
        case ShaderType::VS: return EShLangVertex;
        case ShaderType::FS: return EShLangFragment;
        case ShaderType::CS: return EShLangCompute;
        }

        SKY_UNEXPECTED;
        return EShLangVertex;
    }

    class ShaderIncluder : public glslang::TShader::Includer, public ShaderCompiler::Includer {
    public:
        ShaderIncluder() = default;
        ~ShaderIncluder() override = default;

        IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t depth) override;
        IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t depth) override;
        void releaseInclude(IncludeResult* result) override;

        void AddIncludePath(const std::string &path) { pathList.emplace(path); }

    private:
        std::set<std::string> pathList;
        std::unordered_map<std::string, std::string> sourceMap;
    };

    glslang::TShader::Includer::IncludeResult* ShaderIncluder::includeSystem(const char* headerName, const char* includerName, size_t depth)
    {
        auto iter = sourceMap.find(std::string(headerName));
        if (iter != sourceMap.end()) {
            return new glslang::TShader::Includer::IncludeResult(headerName, iter->second.c_str(), iter->second.size(), nullptr);
        }

        for (const auto &path : pathList) {
            std::filesystem::path fPath(path);
            fPath.append(headerName);
            if (!std::filesystem::exists(fPath)) {
                continue;
            }

            auto [rst, source] = GetShaderSource(fPath.string());
            if (!rst) {
                continue;
            }

            auto res = sourceMap.emplace(headerName, source);
            return new glslang::TShader::Includer::IncludeResult(headerName, res.first->second.c_str(), res.first->second.size(), nullptr);
        }
        return nullptr;
    }

    glslang::TShader::Includer::IncludeResult* ShaderIncluder::includeLocal(const char* headerName, const char* includerName, size_t depth)
    {
        // not supported
        return nullptr;
    }

    void ShaderIncluder::releaseInclude(IncludeResult* result)
    {
        delete result;
    }

    static void SaveSpv(const std::string &path, const std::vector<uint32_t> &spv)
    {
        WriteBin(path, reinterpret_cast<const char *>(spv.data()), spv.size() * sizeof(uint32_t));
    }

    static void SaveGLES(const std::string &path, const std::string &source)
    {
        WriteString(path, source);
    }

    ShaderCompiler::ShaderCompiler()
    {
        glslang::InitializeProcess();
    }

    ShaderCompiler::~ShaderCompiler()
    {
        glslang::FinalizeProcess();
    }

    void ShaderCompiler::AddEngineIncludePath(const std::string &path)
    {
        if (!includer) {
            includer = std::make_unique<ShaderIncluder>();
        }
        auto *shaderIncluder = dynamic_cast<ShaderIncluder*>(includer.get());

        shaderIncluder->AddIncludePath(path);
    }

    bool ShaderCompiler::BuildSpirV(const std::string &path, std::vector<uint32_t> &out, const Option &option)
    {
        auto [rst, source] = GetShaderSource(path);
        if (!rst) {
            return false;
        }

        auto *shaderIncluder = dynamic_cast<ShaderIncluder*>(includer.get());

        const auto *ptr = source.c_str();

        glslang::TShader shader(GetLanguage(option.type));
        shader.setStrings(&ptr, 1);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        shader.setEnvInput(option.language == Language::GLSL ? glslang::EShSourceGlsl : glslang::EShSourceHlsl, shader.getStage(), glslang::EShClientVulkan, 130);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
        if (!shader.parse(GetDefaultResources(), 450, false, EShMsgAST, *shaderIncluder)) {
            LOG_E(TAG, "shader parse failed: path: %s\n, %s\n", path.c_str(), shader.getInfoLog());
            return false;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(EShMsgDefault)) {
            LOG_E(TAG, "link shader failed: %s\n", program.getInfoLog());
            return false;
        }

        glslang::SpvOptions spvOptions = {};
        glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), out, &spvOptions);

        program.buildReflection();

//        LOG_I("%s \n", reinterpret_cast<char*>(out.data()));
        return true;
    }

//    std::string ShaderCompiler::BuildGLES(const std::vector<uint32_t> &spv, const Option &option)
//    {
//        spirv_cross::CompilerGLSL compiler(spv.data(), spv.size());
//        spirv_cross::ShaderResources resources = compiler.get_shader_resources();
//
//        auto remap = [&compiler, &option](auto &resources, bool subpass = false) {
//            for (auto &resource : resources) {
//                if (subpass) {
//                    auto id = compiler.get_decoration(resource.id, spv::DecorationInputAttachmentIndex);
//                    auto loc = option.inputMap[id];
//                    if (loc == 0xFD || loc == 0xFE) {
//                        const std::string name = loc == 0xFD ? "gl_LastFragDepthARM" : "gl_LastFragStencilARM";
//                        compiler.require_extension("GL_ARM_shader_framebuffer_fetch_depth_stencil");
//                        compiler.set_remapped_variable_state(resource.id, true);
//                        compiler.set_name(resource.id, name);
//                        compiler.set_subpass_input_remapped_components(resource.id, 1);
//                    } else {
//                        compiler.remap_ext_framebuffer_fetch(id, option.inputMap[id], true);
//                    }
//                    continue;
//                }
//
//                unsigned set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
//                unsigned binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
//
//                // Modify the decoration to prepare it for GLSL.
//                compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
//
//                // Some arbitrary remapping if we want.
//                compiler.set_decoration(resource.id, spv::DecorationBinding, set * 8 + binding);
//            }
//        };
//
//        remap(resources.uniform_buffers);
//        remap(resources.storage_buffers);
//        remap(resources.sampled_images);
//        remap(resources.storage_images);
//        remap(resources.subpass_inputs, true);
//
//        spirv_cross::CompilerGLSL::Options options;
//        options.version = 320;
//        options.es = true;
//        options.vertex.flip_vert_y = false;
//        compiler.set_common_options(options);
//        auto source = compiler.compile();
//
//        std::string::size_type offset = 0;
//        for (uint32_t i = 0; i < option.outputMap.size(); ++i) {
//            if (option.outputMap[i] == 0xFF) {
//                continue;
//            }
//
//            const char* LAYOUT_PREFIX = "layout(location = ";
//
//            std::stringstream ss1;
//            ss1 << LAYOUT_PREFIX << i << ") out";
//
//            std::stringstream ss2;
//            ss2 << LAYOUT_PREFIX << i << ") inout";
//
//            auto iter = source.find(ss1.str(), offset);
//            if (iter == std::string::npos) {
//                iter = source.find(ss2.str(), offset);
//            }
//
//            if (iter != std::string::npos) {
//                auto loc = iter + strlen(LAYOUT_PREFIX);
//                source[loc] = option.outputMap[i] + '0';
//                offset = loc;
//            }
//        }
//
//        ReplaceAll(source, "_RESERVED_IDENTIFIER_FIXUP_gl_LastFragDepthARM", "gl_LastFragDepthARM");
//        ReplaceAll(source, "_RESERVED_IDENTIFIER_FIXUP_gl_LastFragStencilARM", "gl_LastFragStencilARM");
//        return source;
//    }
//
//    std::string ShaderCompiler::BuildMSL(const std::vector<uint32_t> &spv, const Option &option)
//    {
//        spirv_cross::CompilerMSL compiler(spv.data(), spv.size());
//        spirv_cross::ShaderResources resources = compiler.get_shader_resources();
//
//        auto remap = [&compiler, &option](auto &resources, bool subpass = false) {
//            for (auto &resource : resources) {
//                unsigned set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
//                unsigned binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
//
//                // Modify the decoration to prepare it for GLSL.
////                compiler.unset_decoration(resource.id, spv::DecorationDescriptorSet);
//
//                // Some arbitrary remapping if we want.
////                compiler.set_decoration(resource.id, spv::DecorationBinding, set * 8 + binding);
//            }
//        };
//
//        remap(resources.uniform_buffers);
//        remap(resources.storage_buffers);
//        remap(resources.sampled_images);
//        remap(resources.storage_images);
//        remap(resources.subpass_inputs, true);
//
//        spirv_cross::CompilerMSL::Options options;
//        compiler.set_msl_options(options);
//        return compiler.compile();
//    }

//    void ShaderCompiler::CompileShader(const std::string &path, const Option &option)
//    {
//        std::vector<uint32_t> spv;
//        BuildSpirV(path, option.type, spv);
//        SaveSpv(option.output + ".spv", spv);

//        std::string glslSrc = BuildGLES(spv, option);
//        SaveGLES(option.output + ".gles", glslSrc);
//
//        std::string mslSrc = BuildMSL(spv, option);
//        SaveGLES(option.output + ".msl", mslSrc);
//    }
}
