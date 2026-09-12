//
// Slang -> SPIRV -> RHI shader object creation (Vulkan).
//

#include "SlangBackendTestCommon.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, SlangToRhiShaderObjects)
{
    RunSlangToRhiPipelineTest(GetDevice(), ShaderTarget::SPIRV);
}

TEST_F(AuroraVulkanTest, SlangToRhiComputePipeline)
{
    RunSlangToRhiComputePipelineTest(GetDevice(), ShaderTarget::SPIRV);
}

TEST_F(AuroraVulkanTest, SlangToRhiSpecializedComputePipeline)
{
    const char *source = R"(
#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

[SpecializationConstant]
[[vk::constant_id(0)]]
const int kNumLights = NUM_LIGHTS;

[shader("compute")]
[numthreads(1, 1, 1)]
void mainCS(uint3 tid : SV_DispatchThreadID)
{
    float4 v = float4(float(kNumLights), 0.0, 0.0, 1.0);
}
)";

    ShaderCompilerSlang compiler;
    ShaderCompileDesc csDesc{};
    csDesc.source = source;
    csDesc.entry  = "mainCS";
    csDesc.stage  = ShaderStageFlagBit::CS;
    csDesc.target = ShaderTarget::SPIRV;
    ShaderCompileResult csResult{};
    ASSERT_TRUE(compiler.Compile(csDesc, csResult)) << csResult.errorInfo;

    CounterPtr<ShaderFunction> cs(
        GetDevice()->CreateShaderFunction(MakeShaderFuncDesc(ShaderStageFlagBit::CS, csResult.data)));
    ASSERT_NE(cs.Get(), nullptr);

    ShaderSpecialization spec;
    spec.entries.push_back({0, 8}); // NUM_LIGHTS = 8 via spec constant id 0

    Shader::Descriptor shaderDesc{};
    shaderDesc.cs             = cs.Get();
    shaderDesc.reflection     = &csResult.reflection;
    shaderDesc.specialization = &spec;
    CounterPtr<Shader> shader(GetDevice()->CreateShader(shaderDesc));
    ASSERT_NE(shader.Get(), nullptr);

    ComputePipeline::Descriptor psDesc{};
    psDesc.cs = shader.Get();
    CounterPtr<ComputePipeline> pipeline(GetDevice()->CreatePipelineState(psDesc));
    ASSERT_NE(pipeline.Get(), nullptr);
}
