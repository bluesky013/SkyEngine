//
// Created by blues on 2024/2/18.
//

#include <shader/ShaderCompilerGlsl.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>

#include <core/platform/Platform.h>
#include <core/logger/Logger.h>

#include <shader/ShaderCross.h>

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

    bool ShaderCompilerGlsl::CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result)
    {
        const auto *ptr = desc.source.c_str();
        auto language = GetLanguage(desc.stage);

        glslang::TProgram program;
        glslang::TShader shader(language);
        shader.setStrings(&ptr, 1);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
        shader.setEnvInput(glslang::EShSourceHlsl,  language, glslang::EShClientVulkan, 130);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
        shader.setEntryPoint(desc.entry.c_str());
        if (!shader.parse(GetDefaultResources(), 450, false, EShMsgAST)) {
            LOG_E(TAG, "shader parse failed: path: %s\n", shader.getInfoLog());
            return false;
        }
        program.addShader(&shader);

        if (!program.link(EShMsgDefault)) {
            LOG_E(TAG, "link shader failed: %s\n", program.getInfoLog());
            return false;
        }

        glslang::SpvOptions spvOptions = {};
        spvOptions.stripDebugInfo = true;
        spvOptions.disableOptimizer = false;
        spvOptions.optimizeSize = true;
        glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), result.data, &spvOptions);

        BuildReflectionSPIRV(desc.stage, result);
        return true;
    }

} // namespace sky