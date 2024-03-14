//
// Created by Zach Lee on 2023/2/18.
//

#include <shader/ShaderCompiler.h>

#include <core/file/FileIO.h>
#include <core/template/Overloaded.h>
#include <core/hash/Hash.h>
#include <core/hash/Crc32.h>

#include<boost/tokenizer.hpp>

#include <filesystem>
#include <fstream>
#include <utility>

static const char* TAG = "ShaderCompiler";

namespace sky {
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

    std::pair<bool, std::string> GetSourceFromFile(const std::vector<std::string> &searchPaths, const std::string &path)
    {
        for (const auto &searchPath : searchPaths) {
            std::string loadPath = searchPath + path;
            auto [rst, source] = GetShaderSource(loadPath);
            if (!rst) {
                continue;
            }

            return {true, source};
        }
        return {false, ""};
    }

    std::pair<bool, std::string> ShaderCompiler::ProcessHeaderFile(const std::string &source, ShaderIncludeContext &context, uint32_t depth)
    {
        using LineTokenizer = boost::tokenizer<boost::char_separator<char>>;
        LineTokenizer tok(source, boost::char_separator<char>("\n\r"));
        LineTokenizer::const_iterator iter = tok.begin();

        if (depth > 5) {
            return {false, {}};
        }

        std::string res;

        while (iter != tok.end()) {
            auto line = *iter;
            ++iter;

            if (line.find("#include") == std::string::npos) {
                res += line + '\n';
                continue;
            }

            auto begin = line.find_first_of('\"');
            auto end = line.find_first_of('\"', begin + 1);
            auto fileName = std::string("/") + line.substr(begin + 1, (end - begin - 1));

            auto [ret1, tmpHeader] = GetSourceFromFile(context.searchPaths, fileName);
            if (!ret1) {
                return {false, {}};
            }

            if (context.visited.count(fileName) != 0) {
                continue;
            }

            auto [ret2, headerSource] = ProcessHeaderFile(tmpHeader, context, depth + 1);
            if (!ret2) {
                return { false, {}};
            }
            context.visited.emplace(fileName);
            res += headerSource;
        }

        return {true, res};
    }

    std::pair<bool, std::string> ShaderCompiler::ProcessShaderSource(const std::string &path)
    {
        ShaderIncludeContext context{searchPaths};

        auto [resS, source] = GetSourceFromFile(searchPaths, path);
        if (!resS) {
            return {false, {}};
        }

        auto [resH, final] = ProcessHeaderFile(source, context, 0);
        if (!resH) {
            return {false, {}};
        }

        return {true, final};
    }

    std::string ShaderCompiler::LoadShader(const std::string &path)
    {
        return ProcessShaderSource(path).second;
    }

    ShaderCompiler::ShaderCompiler()
    {
    }

    ShaderCompiler::~ShaderCompiler()
    {
    }

    void ShaderPreprocessor::SetValue(const std::string &key, const MacroValue &val)
    {
        values[key] = val;
        CalculateHash();
    }

    void ShaderPreprocessor::CalculateHash()
    {
        hash = 0;
        for (auto &[key, val] : values) {
            std::visit(Overloaded{
                           [&](const ShaderDef &v) {
                               HashCombine32(hash, static_cast<uint32_t>(v.enable));
                           },
                           [&](const auto &v){
                               HashCombine32(hash, Crc32::Cal(v));
                           }
                       }, val);
        }
    }

//    void ShaderCompiler::BuildSpirV(const std::string &source,
//                                    const std::vector<std::pair<std::string, rhi::ShaderStageFlagBit>> &entries,
//                                    std::vector<std::vector<uint32_t>> &out,
//                                    ShaderReflection &reflection)
//    {
//        glslang::InitializeProcess();
//
//        const auto *ptr = source.c_str();
//
//        glslang::TProgram program;
//        std::vector<std::unique_ptr<glslang::TShader>> shaders(entries.size());
//        out.resize(entries.size());
//
//        uint32_t i = 0;
//        for (const auto &[entry, type] : entries) {
//            shaders[i] = std::make_unique<glslang::TShader>(GetLanguage(type));
//            shaders[i]->setStrings(&ptr, 1);
//            shaders[i]->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
//            shaders[i]->setEnvInput(glslang::EShSourceHlsl, shaders[i]->getStage(), glslang::EShClientVulkan, 130);
//            shaders[i]->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
//            shaders[i]->setEntryPoint(entry.c_str());
//            if (!shaders[i]->parse(GetDefaultResources(), 450, false, EShMsgAST)) {
//                LOG_E(TAG, "shader parse failed: path: %s\n", shaders[i]->getInfoLog());
//                return;
//            }
//            program.addShader(shaders[i].get());
//            ++i;
//        }
//
//        if (!program.link(EShMsgDefault)) {
//            LOG_E(TAG, "link shader failed: %s\n", program.getInfoLog());
//            return;
//        }
//
//        program.buildReflection();
//        int uniformBlock = program.getNumUniformBlocks();
//        for (int index = 0; index < uniformBlock; ++index) {
//            const auto &block = program.getUniformBlock(index);
//            ShaderResource res = {};
//            res.name = block.name;
//            res.visibility = GetVisibility(block.stages);
//            res.set = block.getType()->getQualifier().layoutSet;
//            res.binding = block.getBinding();
//            res.size = block.size;
//
//            auto storage = block.getType()->getQualifier().storage;
//            if (storage == glslang::EvqUniform) {
//                res.type = rhi::DescriptorType::UNIFORM_BUFFER;
//
//                if (!block.getType()->isStruct()) {
//                    continue;
//                }
//                const auto *structType = block.getType()->getStruct();
//                for (int j = 0; j < structType->size(); ++j) {
//                    const auto &member = block.getType()->getStruct()->at(j);
//                    ShaderVariable variable = {};
//                    variable.set = res.set;
//                    variable.binding = res.binding;
//                    variable.name = member.type->getFieldName();
//                    reflection.variables.emplace_back(variable);
//                }
//            } else if (storage == glslang::EvqBuffer) {
//                res.type = rhi::DescriptorType::STORAGE_BUFFER;
//            }
//
//            reflection.resources.emplace_back(res);
//        }
//
//        int uniformVariable = program.getNumUniformVariables();
//        for (int index = 0; index < uniformVariable; ++index) {
//            const auto &var = program.getUniform(index);
//            if (var.getType()->isOpaque()) {
//                printf("uniform var: %s, %d, %d\n", var.name.c_str(), var.stages, var.numMembers);
//                ShaderResource res = {};
//                res.name = var.name;
//
//                const auto *type = var.getType();
//                if (type->isTexture()) {
//                    res.type = rhi::DescriptorType::SAMPLED_IMAGE;
//                } else if (type->isImage()) {
//                    res.type = rhi::DescriptorType::STORAGE_IMAGE;
//                } else if (type->getSampler().sampler) {
//                    res.type = rhi::DescriptorType::SAMPLER;
//                } else if (type->isSubpass()) {
//                    res.type = rhi::DescriptorType::INPUT_ATTACHMENT;
//                }
//
//                res.visibility = GetVisibility(var.stages);
//                res.set = type->getQualifier().layoutSet;
//                res.binding = type->getQualifier().layoutBinding;
//                res.size = 0;
//                reflection.resources.emplace_back(res);
//            } else {
//                auto iter = std::find_if(reflection.variables.begin(), reflection.variables.end(), [&](const auto &p) {
//                    return p.name == var.name;
//                });
//                if (iter != reflection.variables.end()) {
//                    iter->offset = var.offset;
//                }
//            }
//        }
//
//        glslang::SpvOptions spvOptions = {};
//        spvOptions.stripDebugInfo = true;
//        spvOptions.disableOptimizer = false;
//        spvOptions.optimizeSize = true;
//        for (i = 0; i < entries.size(); ++i) {
//            glslang::GlslangToSpv(*program.getIntermediate(shaders[i]->getStage()), out[i], &spvOptions);
//        }
//    }

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
