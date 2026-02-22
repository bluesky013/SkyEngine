//
// Created by Zach Lee on 2023/2/18.
//

#include <shader/ShaderCompiler.h>
#include <shader/ShaderFileSystem.h>

#include <core/file/FileIO.h>
#include <core/template/Overloaded.h>
#include <core/hash/Hash.h>
#include <core/hash/Crc32.h>

#include <boost/tokenizer.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <fstream>
#include <utility>

static const char* TAG = "ShaderCompiler";

namespace sky {
    static std::pair<bool, std::string> GetShaderSource(const FilePath &path)
    {
        std::fstream f = path.OpenFStream(std::ios::binary | std::ios::in);
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

    std::pair<bool, std::string> GetSourceFromFile(const std::vector<FilePath> &searchPaths, const std::string &path)
    {
        for (const auto &searchPath : searchPaths) {
            auto loadPath = searchPath / path;
            auto [rst, source] = GetShaderSource(loadPath);
            if (!rst) {
                continue;
            }

            return {true, source};
        }
        return {false, ""};
    }

    ShaderCompiler::ShaderCompiler() = default;
    ShaderCompiler::~ShaderCompiler() = default;

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
            auto fileName = line.substr(begin + 1, (end - begin - 1));

            auto [ret1, tmpHeader] = GetSourceFromFile(context.searchPaths, fileName);
            if (!ret1) {
                return {false, {}};
            }

            if (context.visited.contains(fileName)) {
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
        const auto &searchPaths = ShaderFileSystem::Get()->GetSearchPaths();
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

    FilePath ShaderCompiler::GetShaderPath(const std::string &name) const
    {
        const auto &searchPaths = ShaderFileSystem::Get()->GetSearchPaths();
        for (const auto &searchPath : searchPaths) {
            auto loadPath = searchPath / name;
            std::fstream f = loadPath.OpenFStream(std::ios::binary | std::ios::in);
            if (!f.is_open()) {
                continue;
            }

            return searchPath;
        }

        return "";
    }

    std::string ShaderCompiler::LoadShader(const std::string &name)
    {
        return ProcessShaderSource(name).second;;
    }

    using ObjectType = rapidjson::Document::Object;
    static ShaderOptionItem LoadShaderOptionItem(const ObjectType &object)
    {
        ShaderOptionItem item = {};

        if (object.HasMember("key")) {
            const char* name = object["key"].GetString();
            item.key = Name(name);
        }
        if (object.HasMember("default")) {
            item.dft = static_cast<uint8_t>(object["default"].GetUint());
        }
        if (object.HasMember("bits")) {
            item.bits = static_cast<uint8_t>(object["bits"].GetUint());
        }
        if (object.HasMember("type")) {
            std::string type = object["type"].GetString();
            if (type == "Pass") {
                item.type = ShaderOptionType::PASS;
            } else if (type == "Batch") {
                item.type = ShaderOptionType::BATCH;
            }
        }

        return item;
    }

    const ShaderOptionEntry* ShaderCompiler::FindPassEntry(const Name& name) const
    {
        auto iter = nameMap.find(name);
        return iter != nameMap.end() ? &passEntries[iter->second] : nullptr;
    }

    void ShaderCompiler::LoadPipelineOptions(const std::string &name)
    {
#if __ANDROID__
        auto [rst, source] = ShaderFileSystem::Get()->LoadCacheSource(Name(name.c_str()));
        std::string optionsData = source;
#else
        std::string optionsData = LoadShader(name);
#endif
        std::vector<ShaderOptionItem> optionItems = PreProcess(optionsData);

        uint32_t currentBit = 0;
        for (const auto &item : optionItems) {
            ShaderOptionEntry entry = {};
            entry.key = item.key;
            entry.dft = item.dft;
            entry.range = {currentBit, currentBit + item.bits - 1};
            nameMap[entry.key] = static_cast<uint32_t>(passEntries.size());
            SKY_ASSERT(item.type == ShaderOptionType::PASS);
            passEntries.emplace_back(entry);
        }
    }

    std::vector<ShaderOptionItem> ShaderCompiler::PreProcess(std::string& source)
    {
        std::vector<ShaderOptionItem> items;

        auto iter = source.find("#pragma option");
        while (iter != std::string::npos) {
            auto end = source.find('\n', iter);

            auto b1 = source.find('(', iter);
            auto b2 = source.find(')', b1);
            if (b1 != std::string::npos && b2 != std::string::npos) {
                std::string optionStr = source.substr( b1 + 1, b2 - b1 - 1);
                rapidjson::Document document;
                document.Parse(optionStr.c_str());

                items.emplace_back(LoadShaderOptionItem(document.GetObject()));
            }
            source.erase(iter, end - iter + 1);
            iter = source.find("#pragma option");
        }
        return items;
    }

    void ShaderCompiler::LoadFromMemory(IInputArchive &archive, ShaderBuildResult &result)
    {
        archive.Load(result.data);

        uint32_t size = 0;
        archive.Load(size);
        result.reflection.resources.resize(size);
        // load resources
        for (uint32_t i = 0; i < size; ++i) {
            auto& res = result.reflection.resources[i];
            archive.Load(res.name);
            archive.Load(res.type);
            archive.Load(res.visibility.value);
            archive.Load(res.set);
            archive.Load(res.binding);
            archive.Load(res.count);
            archive.Load(res.size);
        }

        // load types
        archive.Load(size);
        result.reflection.types.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &type = result.reflection.types[i];
            archive.Load(type.name);
            uint32_t varSize = 0;
            archive.Load(varSize);
            type.variables.resize(varSize);
            for (uint32_t j = 0; j < varSize; ++j) {
                auto &var = type.variables[j];
                archive.Load(var.name);
                archive.Load(var.set);
                archive.Load(var.binding);
                archive.Load(var.offset);
                archive.Load(var.size);
            }
        }

        // load vertex attributes
        archive.Load(size);
        result.reflection.attributes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            auto &attr = result.reflection.attributes[i];
            archive.Load(attr.semantic);
            archive.Load(attr.location);
            archive.Load(attr.vecSize);
            archive.Load(attr.type);
        }
    }

    void ShaderCompiler::SaveToMemory(IOutputArchive& archive, const ShaderBuildResult& result)
    {
        archive.Save(result.data);

        // save resources
        archive.Save(static_cast<uint32_t>(result.reflection.resources.size()));
        for (const auto &res : result.reflection.resources) {
            archive.Save(res.name);
            archive.Save(res.type);
            archive.Save(res.visibility.value);
            archive.Save(res.set);
            archive.Save(res.binding);
            archive.Save(res.count);
            archive.Save(res.size);
        }

        // save types
        archive.Save(static_cast<uint32_t>(result.reflection.types.size()));
        for (const auto &type : result.reflection.types) {
            archive.Save(type.name);
            archive.Save(static_cast<uint32_t>(type.variables.size()));
            for (const auto &var : type.variables) {
                archive.Save(var.name);
                archive.Save(var.set);
                archive.Save(var.binding);
                archive.Save(var.offset);
                archive.Save(var.size);
            }
        }

        // save vertex attributes
        archive.Save(static_cast<uint32_t>(result.reflection.attributes.size()));
        for (const auto &attr : result.reflection.attributes) {
            archive.Save(attr.semantic);
            archive.Save(attr.location);
            archive.Save(attr.vecSize);
            archive.Save(attr.type);
        }

    }

    MD5 ShaderCompiler::CalculateShaderMD5(const std::string &source)
    {
        return MD5::CalculateMD5(source);
    }

    std::string ShaderCompiler::ReplaceShadeName(const Name& name)
    {
        std::string result = name.GetStr().data();
        std::replace(result.begin(), result.end(), '\\', '_');
        std::replace(result.begin(), result.end(), '/', '_');

        return result;
    }

    std::string ShaderCompiler::GetBinaryShaderName(const Name& name, const Name& entry, const ShaderOptionPtr &option)
    {
        auto result = ReplaceShadeName(name);
        auto iter = result.find_last_of('.');
        SKY_ASSERT(iter != std::string::npos); // must be .hlsl

        auto suffix = std::string("_") + std::string(entry.GetStr().data()) + "_" ; // + option;

        return result.insert(iter, suffix);
    }

    rhi::ShaderStageFlagBit ShaderCompiler::GetShaderStage(const std::string& stage)
    {
        if (stage == "vertex") {
            return rhi::ShaderStageFlagBit::VS;
        }
        if (stage == "fragment") {
            return rhi::ShaderStageFlagBit::FS;
        }
        if (stage == "compute") {
            return rhi::ShaderStageFlagBit::CS;
        }

        SKY_ASSERT(0);
        return rhi::ShaderStageFlagBit::VS;
    }

    Name ShaderCompiler::GetTargetName(const ShaderCompileTarget &target)
    {
        switch (target) {
            case ShaderCompileTarget::SPIRV: return Name("SpirV");
            case ShaderCompileTarget::MSL: return Name("Metal");
            case ShaderCompileTarget::DXIL: return Name("DX");
            default:
                break;
        }
        return {};
    }

    void ShaderOption::SetValue(const std::string &key, const uint8_t &val)
    {
        values[key] = val;
        CalculateHash();
    }

    void ShaderOption::CalculateHash()
    {
        hash = 0;
        for (auto &[key, val] : values) {
            HashCombine32(hash, Crc32::Cal(val));
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
