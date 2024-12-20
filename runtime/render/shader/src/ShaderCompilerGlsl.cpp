//
// Created by blues on 2024/2/18.
//

#include <shader/ShaderCompilerGlsl.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/Types.h>

#include <core/platform/Platform.h>
#include <core/logger/Logger.h>

#include <shader/ShaderCross.h>
#include "core/template/Overloaded.h"

static const char* TAG = "ShaderCompilerGlsl";
namespace sky {

    ShaderCompilerGlsl::~ShaderCompilerGlsl()
    {
        glslang::FinalizeProcess();
    }

    bool ShaderCompilerGlsl::Init()
    {
        glslang::InitializeProcess();
        return true;
    }

    static EShLanguage GetLanguage(rhi::ShaderStageFlagBit stage)
    {
        switch (stage) {
            case rhi::ShaderStageFlagBit::VS: return EShLangVertex;
            case rhi::ShaderStageFlagBit::FS: return EShLangFragment;
            case rhi::ShaderStageFlagBit::CS: return EShLangCompute;
            default:
                break;
        }
        SKY_UNEXPECTED;
        return EShLangCount;
    }

    static rhi::BaseType GetBaseType(const glslang::TType* type)
    {
        switch (type->getBasicType()) {
            case glslang::EbtFloat:
                return rhi::BaseType::FLOAT;
            case glslang::EbtInt:
                return rhi::BaseType::INT;
            case glslang::EbtUint:
                return rhi::BaseType::UINT;
            default:
                break;
        }
        return rhi::BaseType::UNDEFINED;
    }

    bool ShaderCompilerGlsl::CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result)
    {
        const auto *ptr = desc.source.c_str();
        std::string finalSource;

        auto language = GetLanguage(desc.stage);

        if (op.option) {
            static const std::string_view PREFIX = "#define ";
            std::string preamble;

            for (auto &val: op.option->values) {
                std::visit(Overloaded{
                        [&](const bool &v) {
                            if (v) {
                                preamble += (std::string(PREFIX) + val.first + "\n");
                            }
                        },
                        [&](const auto &v) {
                        }
                }, val.second);
            }
            finalSource = preamble + desc.source;
            ptr = finalSource.c_str();
        }

        glslang::TProgram program;
        glslang::TShader shader(language);
        shader.setStrings(&ptr, 1);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        shader.setEnvInput(glslang::EShSourceHlsl,  language, glslang::EShClientVulkan, 130);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
        shader.setEntryPoint(desc.entry.c_str());
        if (!shader.parse(GetDefaultResources(), 450, false, EShMessages(EShMsgAST | EShMsgReadHlsl))) {
            LOG_E(TAG, "shader parse failed: %s\n", shader.getInfoLog());
            return false;
        }

        shader.setAutoMapLocations(true);
        program.addShader(&shader);

        if (!program.link(EShMsgDefault)) {
            LOG_E(TAG, "link shader failed: %s\n", program.getInfoLog());
            return false;
        }
        program.buildReflection();

        std::vector<VertexStageAttribute> &attributes = result.reflection.attributes;
        auto num = program.getNumPipeInputs();
        for (int i = 0; i < num && desc.stage == rhi::ShaderStageFlagBit::VS; ++i) {
            const auto &refl = program.getPipeInput(i);
            const auto *type = refl.getType();
            if (type->isBuiltIn()) {
                continue;
            }

            attributes.emplace_back(VertexStageAttribute{
                refl.getType()->getQualifier().semanticName,
                refl.getType()->getQualifier().layoutLocation,
                static_cast<uint32_t>(type->getVectorSize()),
                GetBaseType(type)
            });
        }

        glslang::SpvOptions spvOptions = {};
//        spvOptions.stripDebugInfo = false;
//        spvOptions.disableOptimizer = false;
//        spvOptions.optimizeSize = false;
        glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), result.data, &spvOptions);

        BuildReflectionSPIRV(desc.stage, result);
        return true;
    }

} // namespace sky
