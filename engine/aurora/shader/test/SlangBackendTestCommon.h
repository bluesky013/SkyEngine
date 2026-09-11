//
// Shared slang -> RHI backend test body. Each backend file instantiates this
// with its own fixture and slang target.
//

#pragma once

#include "AuroraTestHelper.h"

#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/PipelineLayout.h>
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

    inline ShaderFunction::Descriptor MakeShaderFuncDesc(ShaderStageFlagBit stage, const std::vector<uint32_t> &blob)
    {
        auto *provider       = new ShaderBinaryProvider();
        provider->binaryData = CounterPtr<BinaryData>(
            new BinaryData(static_cast<uint32_t>(blob.size() * sizeof(uint32_t))));
        std::memcpy(provider->binaryData->Data(), blob.data(),
                    blob.size() * sizeof(uint32_t));

        ShaderFunction::Descriptor desc{};
        desc.stage = stage;
        desc.data  = CounterPtr<ShaderDataProvider>(provider);
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

        CounterPtr<ShaderFunction> vs(
            device->CreateShaderFunction(MakeShaderFuncDesc(ShaderStageFlagBit::VS, vsResult.data)));
        CounterPtr<ShaderFunction> fs(
            device->CreateShaderFunction(MakeShaderFuncDesc(ShaderStageFlagBit::FS, fsResult.data)));
        ASSERT_NE(vs.Get(), nullptr);
        ASSERT_NE(fs.Get(), nullptr);

        Shader::Descriptor shaderDesc{};
        shaderDesc.vs = vs.Get();
        shaderDesc.ps = fs.Get();
        CounterPtr<Shader> shader(device->CreateShader(shaderDesc));
        ASSERT_NE(shader.Get(), nullptr);
    }

} // namespace sky::aurora::test
