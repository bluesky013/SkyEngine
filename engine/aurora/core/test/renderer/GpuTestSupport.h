#pragma once

#include "graph/FrameGraphBuilder.h"

#include "prepare/ScenePrepareScheduler.h"
#include <VulkanCommandPool.h>
#include <VulkanDevice.h>
#include <array>
#include <aurora/rdg/FrameBindings.h>
#include <aurora/rhi/DescriptorEncoder.h>
#include <aurora/rhi/Instance.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <gtest/gtest.h>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    constexpr const char *kGpuResourceAttributes   = R"(
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraFieldAttribute { string path; };
)";
    constexpr const char *kGpuFullscreenSource     = R"(
float4 VSMain(uint i : SV_VertexID) : SV_Position {
    float2 positions[3] = {float2(-1,-1), float2(-1,3), float2(3,-1)};
    return float4(positions[i], 0, 1);
}
float4 Red() : SV_Target { return float4(1,0,0,1); }
float4 Green() : SV_Target { return float4(0,1,0,1); }
)";
    constexpr const char *kGpuComputeSource        = R"(
[AuroraResource(0,0,"fixture/compute")] [[vk::binding(0,0)]] RWStructuredBuffer<uint> data : register(u0);
[numthreads(1,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) { data[id.x] = id.x + 7; }
[numthreads(1,1,1)] void Accumulate(uint3 id : SV_DispatchThreadID) { data[id.x] += 100; }
)";
    constexpr const char *kGpuComputeImageSource   = R"(
[AuroraResource(0,0,"fixture/image")] [[vk::binding(0,0)]] RWTexture2D<float4> outputImage : register(u0);
[numthreads(1,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) { outputImage[id.xy] = float4(0,1,0,1); }
)";
    constexpr const char *kGpuSampleSource         = R"(
[AuroraResource(0,0,"fixture/sample")] [[vk::binding(0,0)]] Texture2D<float4> sourceTex : register(t0);
[AuroraResource(0,1,"fixture/sample")] [[vk::binding(1,0)]] SamplerState sourceSampler : register(s1);
float4 VSMain(uint i : SV_VertexID) : SV_Position {
    float2 positions[3] = {float2(-1,-1), float2(-1,3), float2(3,-1)};
    return float4(positions[i], 0, 1);
}
float4 PSMain(float4 position : SV_Position) : SV_Target { return sourceTex.Sample(sourceSampler, position.xy / 8.0); }
)";
    constexpr const char *kGpuComputeSampleSource  = R"(
[AuroraResource(0,0,"fixture/compute-sample")] [[vk::binding(0,0)]] Texture2D<float4> sourceImage : register(t0);
[AuroraResource(0,1,"fixture/compute-sample")] [[vk::binding(1,0)]] SamplerState sourceSampler : register(s1);
[AuroraResource(1,0,"fixture/compute-sample")] [[vk::binding(0,1)]] RWStructuredBuffer<uint> values : register(u0, space1);
[numthreads(1,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
    values[0] = uint(sourceImage.SampleLevel(sourceSampler, float2(0.5), 0).g * 255);
}
)";
    constexpr const char *kGpuStorageReadSource    = R"(
[AuroraResource(0,0,"fixture/storage-read")] [[vk::binding(0,0)]] RWTexture2D<float4> image : register(u0);
[AuroraResource(0,1,"fixture/storage-read")] [[vk::binding(1,0)]] RWStructuredBuffer<uint> outputData : register(u1);
[AuroraResource(0,2,"fixture/storage-read")] [[vk::binding(2,0)]] StructuredBuffer<uint> inputData : register(t2);
[numthreads(1,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
    outputData[id.x] = inputData[id.x] + uint(image[int2(0,0)].g * 255);
}
)";
    constexpr const char *kGpuRasterStorageSource  = R"(
[AuroraResource(0,0,"fixture/raster-storage")] [[vk::binding(0,0)]] StructuredBuffer<uint> palette : register(t0);
struct Input { uint4 value : COLOR; uint id : SV_VertexID; };
struct Varying { float4 position : SV_Position; float value : TEXCOORD0; };
Varying VSMain(Input input) {
    float2 positions[3] = {float2(-1,-1), float2(-1,3), float2(3,-1)};
    Varying output;
    output.position = float4(positions[input.id], 0, 1);
    output.value = float(input.value.x - input.id + palette[1]);
    return output;
}
float4 PSMain(Varying input) : SV_Target {
    return abs(input.value - float(palette[0] + palette[1])) < 0.1 ? float4(0,1,0,1) : float4(1,0,0,1);
}
)";
    constexpr const char *kGpuComputeUniformSource = R"(
struct First { [AuroraField("first")] uint first; };
struct Second { [AuroraField("second")] uint second; };
[AuroraResource(0,1,"fixture/uniform")] [[vk::binding(1,0)]] ConstantBuffer<First> first : register(b1);
[AuroraResource(0,3,"fixture/uniform")] [[vk::binding(3,0)]] ConstantBuffer<Second> second : register(b3);
[AuroraResource(1,0,"fixture/uniform")] [[vk::binding(0,1)]] RWStructuredBuffer<uint> values : register(u0, space1);
[numthreads(1,1,1)] void CSMain(uint3 id : SV_DispatchThreadID) {
    values[id.x] = first.first + 10 * second.second + id.x;
}
)";
    constexpr const char *kGpuAllocationSource     = R"(
[AuroraResource(0,0,"fixture/allocation")] [[vk::binding(0,0)]] Texture2D<float4> image;
[AuroraResource(1,0,"fixture/allocation")] [[vk::binding(0,1)]] SamplerState sampler;
float4 VSMain(uint i : SV_VertexID) : SV_Position {
    float2 positions[3] = {float2(-1,-1), float2(-1,3), float2(3,-1)};
    return float4(positions[i], 0, 1);
}
float4 PSMain(float4 position : SV_Position) : SV_Target { return image.Sample(sampler, position.xy / 8.0); }
)";
    constexpr const char *kGpuIndexedSource        = R"(
#if AURORA_TARGET_DXIL
#define INDEXED_REGISTER(slot, space) : register(slot, space)
#else
#define INDEXED_REGISTER(slot, space)
#endif
struct ViewParams { [AuroraField("offsetDepth")] float4 offsetDepth; };
struct MaterialParams { [AuroraField("tint")] float4 tint; };
struct ObjectParams { [AuroraField("scale")] float4 scale; };
[AuroraResource(0,0,"fixture/indexed")] [[vk::binding(0,0)]] ConstantBuffer<ViewParams> view INDEXED_REGISTER(b0, space0);
#if MATERIAL_BINDING == 0
[AuroraResource(1,0,"fixture/indexed")] [[vk::binding(0,1)]] ConstantBuffer<MaterialParams> material INDEXED_REGISTER(b0, space1);
#else
[AuroraResource(1,1,"fixture/indexed")] [[vk::binding(1,1)]] ConstantBuffer<MaterialParams> material INDEXED_REGISTER(b1, space1);
#endif
[AuroraResource(2,0,"fixture/indexed")] [[vk::binding(0,2)]] ConstantBuffer<ObjectParams> object INDEXED_REGISTER(b0, space2);
struct Vertex { float2 position : POSITION; float4 color : COLOR; };
struct Varying { float4 position : SV_Position; float4 color : COLOR; };
Varying VSMain(Vertex input) {
    Varying output;
    output.position = float4(input.position * object.scale.xy + view.offsetDepth.xy, view.offsetDepth.z, 1);
    output.color = input.color;
    return output;
}
float4 FSMain(Varying input) : SV_Target { return input.color * material.tint; }
)";
    ShaderFunctionPtr     CompileGpuFunction(Device &device, const std::string &source, ShaderStageFlagBit stage, const char *entry)
    {
        ShaderCompilerSlang compiler;
        ShaderCompileDesc   descriptor{};
        descriptor.source = kGpuResourceAttributes + source;
        descriptor.entry  = entry;
        descriptor.stage  = stage;
        descriptor.target = device.GetAPI() == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;
        ShaderCompileResult result;
        if (!compiler.Compile(descriptor, result)) {
            ADD_FAILURE() << result.errorInfo;
            return {};
        }
        ShaderFunction::Descriptor function{};
        function.stage    = stage;
        function.artifact = std::make_shared<const ShaderStageArtifact>(std::move(result));
        return ShaderFunctionPtr(device.CreateShaderFunction(function));
    }
    std::vector<ShaderBindingPolicy> ComputeUniformPolicies(bool dynamic)
    {
        return dynamic ? std::vector<ShaderBindingPolicy>{{0, 1}, {0, 3}} : std::vector<ShaderBindingPolicy>{};
    }
    std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr>
    CustomStorageReadGraph(Device &device, ShaderPtr writer, ShaderPtr bufferWriter, const ShaderPtr &reader)
    {
        class Callback final : public FrameCustomPassEncoder {
        public:
            CounterPtr<ComputePipeline> pipeline;
            CounterPtr<ResourceGroup>   group;
            FrameOperationResult        Encode(FrameCustomPassContext &context) const noexcept override
            {
                auto encoder = context.command.CreateComputeEncoder();
                encoder->BindPipeline(pipeline.Get());
                encoder->BindResourceGroup(0, group.Get());
                encoder->Dispatch(4, 1, 1);
                return FrameOperationComplete{};
            }
        };
        FrameGraphImage image;
        image.descriptor.extent = {8, 8, 1};
        image.descriptor.format = PixelFormat::RGBA8_UNORM;
        image.descriptor.usage  = ImageUsageFlagBit::STORAGE;
        image.final             = ImageLayout::GENERAL;
        image.image             = ImagePtr(device.CreateImage(image.descriptor));
        Buffer::Descriptor desc;
        desc.size          = 16;
        desc.usage         = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC;
        auto output        = BufferPtr(device.CreateBuffer(desc));
        auto input         = BufferPtr(device.CreateBuffer(desc));
        desc.usage         = BufferUsageFlagBit::TRANSFER_DST;
        desc.memory        = MemoryType::GPU_TO_CPU;
        auto readback      = BufferPtr(device.CreateBuffer(desc));
        auto callback      = std::make_shared<Callback>();
        callback->pipeline = CounterPtr<ComputePipeline>(device.CreatePipelineState(ComputePipeline::Descriptor{reader.Get()}));
        callback->group    = CounterPtr<ResourceGroup>(device.CreateResourceGroup({reader.Get(), 0}));
        if (!image.image || !input || !output || !readback || !callback->pipeline || !callback->group) {
            ADD_FAILURE() << "Custom storage-read allocation failed";
            return {};
        }
        auto descriptor = callback->group->CreateEncoder();
        if (!descriptor || !descriptor->WriteImage(0, image.image.Get(), ImageLayout::GENERAL, 0) ||
            !descriptor->WriteBuffer(1, output.Get(), 0, 16, 0) || !descriptor->WriteBuffer(2, input.Get(), 0, 16, 0) || !descriptor->End()) {
            ADD_FAILURE() << "Custom storage-read descriptor encoding failed";
            return {};
        }
        FrameGraphBuilder builder;
        const auto        source  = std::get<GraphImageHandle>(builder.Import(std::move(image)));
        const auto        storage = std::get<GraphBufferHandle>(builder.Import(output, CopyBufferState::COMMON, CopyBufferState::COMMON, false));
        const auto target      = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        const auto inputHandle = std::get<GraphBufferHandle>(builder.Import(input, CopyBufferState::COMMON, CopyBufferState::COMMON, false));
        FrameComputePass    writeBuffer;
        ComputeBindingDraft writeBufferBindings;
        writeBuffer.shader  = std::move(bufferWriter);
        writeBuffer.groupsX = 4;
        writeBufferBindings.buffers.push_back({0, 0, {inputHandle, 0, 16}, 0, FrameAccessMode::WRITE});
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(writeBuffer, std::move(writeBufferBindings))));
        builder.Compute(std::move(writeBuffer));
        FrameComputePass    write;
        ComputeBindingDraft writeBindings;
        write.shader  = std::move(writer);
        write.groupsX = write.groupsY = 8;
        writeBindings.images.push_back({0, 0, source, {}, FrameAccessMode::WRITE});
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(write, std::move(writeBindings))));
        builder.Compute(std::move(write));
        FrameCustomPass read;
        read.encoder = std::move(callback);
        read.images.push_back({0, 0, source, {}, FrameAccessMode::READ, FrameImageBindingKind::STORAGE});
        read.buffers.push_back({0, 1, {storage, 0, 16}, 0, FrameAccessMode::WRITE, FrameBufferBindingKind::STORAGE});
        read.buffers.push_back({0, 2, {inputHandle, 0, 16}, 0, FrameAccessMode::READ, FrameBufferBindingKind::STORAGE});
        builder.Custom(std::move(read));
        builder.Copy(BufferCopyPass{storage, target, 0, 0, 16});
        builder.Export({target, 0, 16});
        return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), readback};
    }
    std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr> ComputeUniformGraph(Device &device, ShaderPtr shader, bool dynamic)
    {
        auto create = [&](uint64_t size, MemoryType memory, BufferUsageFlags usage) {
            Buffer::Descriptor descriptor;
            descriptor.size   = size;
            descriptor.memory = memory;
            descriptor.usage  = usage;
            return BufferPtr(device.CreateBuffer(descriptor));
        };
        auto upload   = create(1024, MemoryType::CPU_TO_GPU, BufferUsageFlagBit::TRANSFER_SRC);
        auto uniform  = create(1024, MemoryType::GPU_ONLY, BufferUsageFlagBit::UNIFORM | BufferUsageFlagBit::TRANSFER_DST);
        auto storage  = create(64, MemoryType::GPU_ONLY, BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::TRANSFER_SRC);
        auto readback = create(64, MemoryType::GPU_TO_CPU, BufferUsageFlagBit::TRANSFER_DST);
        if (!upload || !uniform || !storage || !readback) {
            ADD_FAILURE() << "Compute uniform buffer allocation failed";
            return {};
        }
        std::array<uint32_t, 256> values{};
        values[64]   = 11;
        values[128]  = 7;
        auto *mapped = upload->Map();
        if (!mapped) {
            ADD_FAILURE() << "Compute uniform upload map failed";
            return {};
        }
        std::memcpy(mapped, values.data(), sizeof(values));
        upload->UnMap();
        FrameGraphBuilder builder;
        const auto source    = std::get<GraphBufferHandle>(builder.Import(upload, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
        const auto constants = std::get<GraphBufferHandle>(builder.Import(uniform, CopyBufferState::COMMON, CopyBufferState::SHADER_READ, false));
        const auto output = std::get<GraphBufferHandle>(builder.Import(storage, CopyBufferState::COMMON, CopyBufferState::UNORDERED_ACCESS, false));
        const auto host   = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, constants, 0, 0, 1024})));
        FrameComputePass    pass;
        ComputeBindingDraft passBindings;
        pass.shader  = std::move(shader);
        pass.groupsX = 4;
        for (const auto binding : {3u, 1u}) {
            const uint32_t offset = binding == 3 ? 512 : 256;
            passBindings.buffers.push_back({0, binding, {constants, dynamic ? 0u : offset, 16}, dynamic ? offset : 0, FrameAccessMode::READ});
        }
        passBindings.buffers.push_back({1, 0, {output, 0, 16}, 0, FrameAccessMode::WRITE});
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(pass, std::move(passBindings))));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(pass))));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({output, host, 0, 0, 16})));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({host, 0, 16})));
        return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), std::move(readback)};
    }
    std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr>
    RepeatedComputeUniformGraph(Device &device, ShaderPtr shader, bool dynamic, bool changeStaticRange = false)
    {
        auto [graph, readback] = ComputeUniformGraph(device, std::move(shader), dynamic);
        if (!graph)
            return {};
        auto  data        = graph->GetData();
        auto &first       = std::get<FrameComputePass>(data.passes[1]);
        first.sideEffect  = true;
        auto second       = first;
        second.sideEffect = false;
        for (auto &use : second.groups)
            if (use.group)
                use.group = std::make_shared<const FrameBindingGroup>(use.group->GetData());
        if (dynamic) {
            second.groups[0].bufferUses = {{1, 512}, {3, 256}};
        } else if (changeStaticRange) {
            auto group = second.groups[0].group->GetData();
            for (auto &binding : group.buffers)
                binding.range.offset = binding.binding == 1 ? 512 : 256;
            second.groups[0].group = std::make_shared<const FrameBindingGroup>(std::move(group));
        }
        data.passes.insert(data.passes.begin() + 2, std::move(second));
        return {std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data)), std::move(readback)};
    }
    std::pair<std::shared_ptr<const FrozenFrameGraph>, BufferPtr> RasterStorageGraph(Device &device, bool computeProducer)
    {
        const auto vs = CompileGpuFunction(device, kGpuRasterStorageSource, ShaderStageFlagBit::VS, "VSMain");
        const auto fs = CompileGpuFunction(device, kGpuRasterStorageSource, ShaderStageFlagBit::FS, "PSMain");
        if (!vs || !fs)
            return {};
        Shader::Descriptor shaderDesc;
        shaderDesc.vs             = vs.Get();
        shaderDesc.ps             = fs.Get();
        const auto         shader = ShaderPtr(device.CreateShader(shaderDesc));
        Buffer::Descriptor storageDesc;
        storageDesc.size           = 64;
        storageDesc.usage          = BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::TRANSFER_DST;
        const auto         storage = BufferPtr(device.CreateBuffer(storageDesc));
        Buffer::Descriptor hostDesc{64, BufferUsageFlagBit::TRANSFER_SRC, MemoryType::CPU_TO_GPU};
        const auto         upload = BufferPtr(device.CreateBuffer(hostDesc));
        hostDesc.size             = 256;
        hostDesc.usage            = BufferUsageFlagBit::TRANSFER_DST;
        hostDesc.memory           = MemoryType::GPU_TO_CPU;
        auto            readback  = BufferPtr(device.CreateBuffer(hostDesc));
        FrameGraphImage target;
        target.descriptor.format = PixelFormat::RGBA8_UNORM;
        target.descriptor.extent = {8, 8, 1};
        target.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
        target.image             = ImagePtr(device.CreateImage(target.descriptor));
        if (!shader || !storage || !upload || !readback || !target.image)
            return {};
        std::array<uint32_t, 16> values;
        for (uint32_t i = 0; i < values.size(); ++i)
            values[i] = i + 7;
        auto *mapped = upload->Map();
        if (!mapped)
            return {};
        std::memcpy(mapped, values.data(), sizeof(values));
        upload->UnMap();
        FrameGraphBuilder builder;
        const auto        buffer = std::get<GraphBufferHandle>(builder.Import(storage, CopyBufferState::COMMON, CopyBufferState::COMMON, false));
        if (computeProducer) {
            const auto         cs = CompileGpuFunction(device, kGpuComputeSource, ShaderStageFlagBit::CS, "CSMain");
            Shader::Descriptor computeDesc;
            computeDesc.cs = cs.Get();
            FrameComputePass    compute;
            ComputeBindingDraft computeBindings;
            compute.shader = cs ? ShaderPtr(device.CreateShader(computeDesc)) : ShaderPtr{};
            if (!compute.shader)
                return {};
            compute.groupsX = 16;
            computeBindings.buffers.push_back({0, 0, {buffer, 0, 64}, 0, FrameAccessMode::WRITE});
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(SetComputeBindings(compute, std::move(computeBindings))));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Compute(std::move(compute))));
        } else {
            const auto source = std::get<GraphBufferHandle>(builder.Import(upload, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({source, buffer, 0, 0, 64})));
        }
        const auto         color = std::get<GraphImageHandle>(builder.Import(std::move(target)));
        RasterDraw         draw;
        RasterBindingDraft drawBindings;
        draw.shader  = shader;
        draw.command = {3, 1, 0, 0};
        draw.vertices.push_back({{buffer, 0, 64}, 4});
        draw.attributes.push_back({0, 0, 0, Format::U_RGBA8, "COLOR", 0});
        drawBindings.storage.push_back({0, 0, {buffer, 0, 64}, 0});
        FrameRasterPass raster;
        raster.color = color;
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(SetRasterBindings(draw, std::move(drawBindings))));
        raster.draws.push_back(std::move(draw));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Raster(std::move(raster))));
        const auto      host = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
        BufferImageCopy region;
        region.imageExtent = {8, 8, 1};
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({host, color, region, ImageCopyDirection::IMAGE_TO_BUFFER})));
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({host, 0, 256})));
        return {std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze()), std::move(readback)};
    }
    std::weak_ptr<const void> TrackGraphGpuPayload(std::shared_ptr<const FrozenFrameGraph> &graph)
    {
        auto data                      = graph->GetData();
        data.payload                   = std::make_shared<std::shared_ptr<const void>>(data.payload);
        std::weak_ptr<const void> weak = data.payload;
        graph                          = std::make_shared<const FrozenFrameGraph>(graph->GetId(), std::move(data));
        return weak;
    }
    struct Finally {
        std::function<void()> fn;
        ~Finally()
        {
            fn();
        }
    };
    FenceQueryResult Await(Fence &fence)
    {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        auto       result   = fence.Query();
        while (result.status == FenceStatus::PENDING && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
            result = fence.Query();
        }
        return result;
    }
    std::shared_ptr<const PrepareProducts> PreparePayload(uint32_t seed)
    {
        constexpr SceneId scene{0, 1};
        CpuSceneStore     store;
        FramePacket       packet;
        packet.sequence = 1;
        packet.commands.emplace_back(CreateScene{scene, {}});
        for (uint32_t i = 0; i < 2; ++i) {
            RenderEntityDesc object;
            object.world.v[12] = static_cast<float>(seed + i * 7);
            packet.commands.emplace_back(CreateRenderEntity{{scene, {i, 1}}, object});
        }
        if (!store.Apply(packet).Succeeded())
            return {};
        auto scheduler = ScenePrepareScheduler::Create({2});
        if (!scheduler.scheduler)
            return {};
        auto       frame = store.Freeze();
        SystemDesc system{1, "GpuProbePayload"};
        system.accesses = {ComponentAccess<RenderTransform>("scene", SceneAccessOwner(scene), AccessMode::READ)};
        auto submitted =
            scheduler.scheduler->Submit(*frame, {system},
                                        [scene](const FrozenPrepareContext &context) {
                                            context.Read<RenderTransform>(scene).ForEach([&](RenderEntityId, const RenderTransform &transform) {
                                                std::array<uint32_t, 16> values;
                                                for (uint32_t i = 0; i < values.size(); ++i)
                                                    values[i] = static_cast<uint32_t>(transform.world.v[12]) * 31 + i;
                                                EXPECT_TRUE(context.Emit(1, std::as_bytes(std::span{values})));
                                            });
                                        },
                                        {{1, scene, 1}}, {{1, 1, 64, 0}});
        if (!submitted.Accepted())
            return {};
        const auto &result = submitted.group->Wait();
        return result.Succeeded() ? result.products : nullptr;
    }
    void HostReadBarrier(Device *device, CommandBuffer *command)
    {
        if (device->GetAPI() != API::VULKAN)
            return;
        // Probe-only native host visibility; the future graph compiler must emit this dependency itself.
        VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.memoryBarrierCount = 1;
        info.pMemoryBarriers    = &barrier;
        static_cast<VulkanDevice *>(device)->GetDeviceFn().vkCmdPipelineBarrier2(static_cast<VulkanCommandBuffer *>(command)->GetNativeHandle(),
                                                                                 &info);
    }
} // namespace
