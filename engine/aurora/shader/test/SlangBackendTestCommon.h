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

#include <array>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <chrono>
#include <cstring>
#include <thread>

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

    inline void RunStageVariantReadback(Device *device, ShaderTarget target)
    {
        ShaderCompilerSlang                               compiler;
        const std::array                                  sources{R"(
#ifndef VERTEX_VALUE
#define VERTEX_VALUE 2
#endif
#if AURORA_TARGET_DXIL
static const uint kValue = VERTEX_VALUE;
#else
[SpecializationConstant] [[vk::constant_id(0)]] const uint kValue = VERTEX_VALUE;
#endif
struct Vertex { float4 position : SV_Position; float color : TEXCOORD0; };
[shader("vertex")] Vertex Main(uint id : SV_VertexID) {
    Vertex o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv * 2 - 1, 0, 1);
    o.color = float(kValue) * 0.1;
    return o;
})",
                                 R"(
#ifndef FRAGMENT_VALUE
#define FRAGMENT_VALUE -3
#endif
#if AURORA_TARGET_DXIL
static const int kValue = FRAGMENT_VALUE;
#else
[SpecializationConstant] [[vk::constant_id(0)]] const int kValue = FRAGMENT_VALUE;
#endif
struct Vertex { float4 position : SV_Position; float color : TEXCOORD0; };
[shader("fragment")] float4 Main(Vertex input) : SV_Target {
    return float4(input.color, float(kValue + 4) * 0.1, 0, 1);
})"};
        std::array<ShaderPtr, 2>                          shaders;
        std::array<std::array<ShaderCompileResult, 2>, 2> artifacts;
        for (size_t selection = 0; selection < 2; ++selection) {
            ShaderSpecialization             specialization;
            std::array<ShaderFunctionPtr, 2> functions;
            for (size_t stage = 0; stage < 2; ++stage) {
                const Name          key(stage == 0 ? "VERTEX_VALUE" : "FRAGMENT_VALUE");
                const auto          type         = stage == 0 ? ShaderVariantType::UINT32 : ShaderVariantType::INT32;
                const auto          defaultValue = stage == 0 ? 2u : static_cast<uint32_t>(-3);
                ShaderVariantSchema schema;
                schema.sources           = {{Name("shader"), 0, 8}};
                schema.entries           = {{key, Name("shader"), 0, 8, defaultValue}};
                schema.entries[0].type   = type;
                schema.entries[0].symbol = Name("kValue");
                schema.totalBits         = 8;
                ShaderVariant values;
                values.entries = {{key, selection == 0 ? defaultValue : (stage == 0 ? 7u : 4u), type}};
                ShaderCompileDesc desc;
                desc.source    = sources[stage];
                desc.entry     = "Main";
                desc.stage     = stage == 0 ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
                desc.target    = target;
                desc.schema    = &schema;
                desc.variant   = &values;
                auto &artifact = artifacts[selection][stage];
                ASSERT_TRUE(compiler.Compile(desc, artifact)) << artifact.errorInfo;
                std::string error;
                ASSERT_TRUE(BuildShaderSpecialization(&values, artifact, specialization, error)) << error;
                functions[stage] = device->CreateShaderFunction(MakeShaderFuncDesc(desc.stage, artifact));
                ASSERT_TRUE(functions[stage]);
            }
            Shader::Descriptor desc;
            desc.vs             = functions[0].Get();
            desc.ps             = functions[1].Get();
            desc.specialization = &specialization;
            shaders[selection]  = device->CreateShader(desc);
            ASSERT_TRUE(shaders[selection]);
        }
        for (size_t stage = 0; stage < 2; ++stage)
            EXPECT_EQ(artifacts[0][stage].data == artifacts[1][stage].data, target != ShaderTarget::DXIL);
        Image::Descriptor imageDesc;
        imageDesc.format = PixelFormat::RGBA8_UNORM;
        imageDesc.extent = {128, 4, 1};
        imageDesc.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
        ImagePtr image(device->CreateImage(imageDesc));
        ASSERT_TRUE(image);
        Buffer::Descriptor bufferDesc;
        bufferDesc.size   = 128 * 4 * 4;
        bufferDesc.usage  = BufferUsageFlagBit::TRANSFER_DST;
        bufferDesc.memory = target == ShaderTarget::DXIL ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
        BufferPtr readback(device->CreateBuffer(bufferDesc));
        ASSERT_TRUE(readback);
        FrameGraphData data;
        data.buffers.push_back({readback, bufferDesc.size, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false});
        FrameGraphImage output;
        output.image      = FrameImageReference(image);
        output.descriptor = imageDesc;
        output.initial    = ImageLayout::UNDEFINED;
        output.final      = ImageLayout::TRANSFER_SRC;
        data.images.push_back(output);
        FrameRasterPass pass;
        pass.color = {1, 0};
        for (uint32_t index = 0; index < shaders.size(); ++index) {
            RasterDraw draw;
            draw.shader              = shaders[index];
            draw.command.vertexCount = 3;
            draw.scissor             = {{static_cast<int32_t>(index * 64), 0}, {64, 4}};
            pass.draws.push_back(std::move(draw));
        }
        data.passes.emplace_back(std::move(pass));
        BufferImageCopyPass copy;
        copy.buffer                 = {1, 0};
        copy.image                  = {1, 0};
        copy.direction              = ImageCopyDirection::IMAGE_TO_BUFFER;
        copy.region.imageExtent     = imageDesc.extent;
        copy.region.subRange.layers = 1;
        data.passes.emplace_back(copy);
        data.exports.push_back({{1, 0}, 0, bufferDesc.size});
        auto                                graph = std::make_shared<const FrozenFrameGraph>(1, std::move(data));
        std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
        ASSERT_TRUE(context);
        const auto begun = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
        auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
        if (const auto *error = std::get_if<FrameError>(&compiled))
            FAIL() << error->message << " code=" << static_cast<uint32_t>(error->code);
        auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
        const auto dispatched = context->Dispatch(plan);
        ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
        auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
        auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        bool       completed = false;
        const auto deadline  = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!completed && std::chrono::steady_clock::now() < deadline) {
            std::array<FrameCompletion, 1> completion{};
            if (context->PollCompleted(completion) != 0)
                completed = completion[0].status == FrameCompletionStatus::COMPLETE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        ASSERT_TRUE(completed);
        const auto *pixels = readback->Map();
        ASSERT_TRUE(pixels);
        for (uint32_t index = 0; index < 2; ++index) {
            const auto offset = (2 * 128 + index * 64 + 32) * 4;
            EXPECT_NEAR(pixels[offset], index == 0 ? 51.f : 178.5f, 1);
            EXPECT_NEAR(pixels[offset + 1], index == 0 ? 25.5f : 204.f, 1);
            EXPECT_EQ(pixels[offset + 3], 255u);
        }
        readback->UnMap();
    }

} // namespace sky::aurora::test
