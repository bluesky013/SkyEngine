//
// Shared slang -> RHI backend test body. Each backend file instantiates this
// with its own fixture and slang target.
//

#pragma once

#include "AuroraTestHelper.h"

#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <core/archive/BinaryData.h>

#include <cstring>

namespace sky::aurora::test {

    inline const char *kSlangFullscreenVs = R"(
struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput mainVS(uint vertexID : SV_VertexID)
{
    VSOutput o;
    o.uv       = float2((vertexID << 1) & 2, vertexID & 2);
    o.position = float4(o.uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}
)";

    inline const char *kSlangSolidColorFs = R"(
struct FSOutput
{
    float4 color : SV_Target;
};

[shader("fragment")]
FSOutput mainFS(float2 uv : TEXCOORD0)
{
    FSOutput o;
    o.color = float4(1.0, 0.0, 0.0, 1.0);
    return o;
}
)";

    inline const char *kSlangComputeCs = R"(
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraFieldAttribute { string path; };
struct Params {
    [AuroraField("viewProj")] float4x4 viewProj;
    [AuroraField("color")] float4 color;
};
[AuroraResource(1,0,"fixture/compute")] [[vk::binding(0,1)]] ConstantBuffer<Params> gParams;

[shader("compute")]
[numthreads(1, 1, 1)]
void mainCS(uint3 tid : SV_DispatchThreadID)
{
    float4 v = gParams.color;
}
)";

    inline ShaderFunction::Descriptor MakeShaderFuncDesc(ShaderStageFlagBit stage, const ShaderCompileResult &compiled)
    {
        ShaderFunction::Descriptor desc{};
        desc.stage    = stage;
        desc.artifact = std::make_shared<const ShaderStageArtifact>(compiled);
        return desc;
    }

    // shared flow: slang compile (VS+FS) -> RHI ShaderFunction/Shader objects
    inline void RunSlangToRhiPipelineTest(Device *device, ShaderTarget target)
    {
        ShaderCompilerSlang compiler;

        ShaderCompileDesc vsDesc{};
        vsDesc.source = kSlangFullscreenVs;
        vsDesc.entry  = "mainVS";
        vsDesc.stage  = ShaderStageFlagBit::VS;
        vsDesc.target = target;
        ShaderCompileResult vsResult{};
        ASSERT_TRUE(compiler.Compile(vsDesc, vsResult)) << vsResult.errorInfo;
        ASSERT_FALSE(vsResult.data.empty());

        ShaderCompileDesc fsDesc{};
        fsDesc.source = kSlangSolidColorFs;
        fsDesc.entry  = "mainFS";
        fsDesc.stage  = ShaderStageFlagBit::FS;
        fsDesc.target = target;
        ShaderCompileResult fsResult{};
        ASSERT_TRUE(compiler.Compile(fsDesc, fsResult)) << fsResult.errorInfo;
        ASSERT_FALSE(fsResult.data.empty());

        CounterPtr<ShaderFunction> vs(device->CreateShaderFunction(MakeShaderFuncDesc(ShaderStageFlagBit::VS, vsResult)));
        CounterPtr<ShaderFunction> fs(device->CreateShaderFunction(MakeShaderFuncDesc(ShaderStageFlagBit::FS, fsResult)));
        ASSERT_NE(vs.Get(), nullptr);
        ASSERT_NE(fs.Get(), nullptr);

        Shader::Descriptor shaderDesc{};
        shaderDesc.vs = vs.Get();
        shaderDesc.ps = fs.Get();
        CounterPtr<Shader> shader(device->CreateShader(shaderDesc));
        ASSERT_NE(shader.Get(), nullptr);
    }

    // shared flow: slang compile (CS) -> RHI shader (with reflection) -> compute PSO.
    // The native pipeline layout / root signature is derived from the reflection.
    inline void RunSlangToRhiComputePipelineTest(Device *device, ShaderTarget target)
    {
        ShaderCompilerSlang compiler;

        ShaderCompileDesc csDesc{};
        csDesc.source = kSlangComputeCs;
        csDesc.entry  = "mainCS";
        csDesc.stage  = ShaderStageFlagBit::CS;
        csDesc.target = target;
        ShaderCompileResult csResult{};
        ASSERT_TRUE(compiler.Compile(csDesc, csResult)) << csResult.errorInfo;
        ASSERT_FALSE(csResult.data.empty());

        CounterPtr<ShaderFunction> cs(device->CreateShaderFunction(MakeShaderFuncDesc(ShaderStageFlagBit::CS, csResult)));
        ASSERT_NE(cs.Get(), nullptr);

        Shader::Descriptor shaderDesc{};
        shaderDesc.cs = cs.Get();
        CounterPtr<Shader> shader(device->CreateShader(shaderDesc));
        ASSERT_NE(shader.Get(), nullptr);

        ComputePipeline::Descriptor psDesc{};
        psDesc.cs = shader.Get();
        CounterPtr<ComputePipeline> pipeline(device->CreatePipelineState(psDesc));
        ASSERT_NE(pipeline.Get(), nullptr);
    }

} // namespace sky::aurora::test
