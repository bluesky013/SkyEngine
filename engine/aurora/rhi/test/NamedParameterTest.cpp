#include "AuroraTestHelper.h"
#include "ShaderArtifactTestHelper.h"

#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/ResourceGroup.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
namespace {
    ShaderBlockLayout MaterialBlock()
    {
        ShaderBlockLayout block;
        block.name       = "Material";
        block.structName = "MaterialData";
        block.binding    = 3;
        block.size       = 32;
        block.alignment  = 16;
        ShaderBlockMember tint;
        tint.name       = "tint";
        tint.size       = 16;
        tint.alignment  = 16;
        tint.scalarType = ShaderScalarType::FLOAT;
        tint.kind       = ShaderTypeKind::VECTOR;
        tint.cols       = 4;
        ShaderBlockMember gain;
        gain.name       = "gain";
        gain.offset     = 16;
        gain.size       = 4;
        gain.alignment  = 4;
        gain.scalarType = ShaderScalarType::FLOAT;
        gain.kind       = ShaderTypeKind::SCALAR;
        block.members   = {tint, gain};
        return block;
    }

    ShaderPtr MakeNamedShader(Device *device, bool revision = false, bool duplicateAliases = false)
    {
        std::string source = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct MaterialData { float4 tint; float gain; };
struct OptionalData { float value; };
kernel void CSMain(device float4 *output [[buffer(0)]], constant MaterialData &material [[buffer(2)]],
                   constant OptionalData &optional [[buffer(4)]], texture2d<float> unusedTexture [[texture(1)]],
                   sampler unusedSampler [[sampler(3)]], device float4 *unusedBuffer [[buffer(6)]]) {
    output[0] = material.tint * material.gain;
}
)MSL";
        if (revision)
            source += "\n";
        ShaderReflection reflection;
        ShaderResource   material;
        material.name             = "Material";
        material.type             = ShaderResourceType::UNIFORM_BUFFER;
        material.binding          = 3;
        material.stages           = ShaderStageFlagBit::CS;
        material.declarationOwner = "rhi-test/named-parameters";
        ShaderResource output;
        output.name               = "Output";
        output.type               = ShaderResourceType::STORAGE_BUFFER;
        output.access             = ShaderResourceAccess::READ_WRITE;
        output.bufferKind         = ShaderBufferKind::STRUCTURED;
        output.elementStride      = 16;
        output.stages             = ShaderStageFlagBit::CS;
        output.declarationOwner   = material.declarationOwner;
        reflection.resources      = {material, output};
        reflection.blocks         = {MaterialBlock()};
        reflection.nativeBindings = {{0, 3, ShaderStageFlagBit::CS, 2, ShaderNativeCategory::METAL_BUFFER},
                                     {0, 0, ShaderStageFlagBit::CS, 0, ShaderNativeCategory::METAL_BUFFER}};
        auto optional             = material;
        optional.name             = "Optional";
        optional.binding          = 4;
        auto optionalBlock        = MaterialBlock();
        optionalBlock.name        = "Optional";
        optionalBlock.structName  = "OptionalData";
        optionalBlock.binding     = 4;
        optionalBlock.size = optionalBlock.alignment = 4;
        optionalBlock.members                        = {optionalBlock.members[1]};
        optionalBlock.members[0].name                = "value";
        optionalBlock.members[0].offset              = 0;
        ShaderResource texture;
        texture.name             = "UnusedTexture";
        texture.type             = ShaderResourceType::SAMPLED_IMAGE;
        texture.binding          = 9;
        texture.texture          = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
        texture.declarationOwner = material.declarationOwner;
        ShaderResource sampler;
        sampler.name                  = "UnusedSampler";
        sampler.type                  = ShaderResourceType::SAMPLER;
        sampler.binding               = 11;
        sampler.sampler               = ShaderSamplerType::REGULAR;
        sampler.declarationOwner      = material.declarationOwner;
        auto unusedBuffer             = output;
        unusedBuffer.name             = "UnusedBuffer";
        unusedBuffer.binding          = 12;
        reflection.declarations       = {material, output, optional, texture, sampler, unusedBuffer};
        reflection.declaredBlocks     = {MaterialBlock(), optionalBlock};
        reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
        auto artifact =
            MakeTestShaderArtifact(ShaderTarget::MSL, ShaderStageFlagBit::CS, {reinterpret_cast<const uint8_t *>(source.data()), source.size()},
                                   "CSMain", reflection, "rhi-test/named-parameters.metal");
        if (!artifact)
            return {};
        ShaderFunction::Descriptor function{};
        function.stage    = ShaderStageFlagBit::CS;
        function.artifact = std::move(artifact);
        auto compute      = ShaderFunctionPtr(device->CreateShaderFunction(function));
        if (!compute)
            return {};
        Shader::Descriptor descriptor{};
        descriptor.cs               = compute.Get();
        descriptor.parameterAliases = {{"tintAlias", "Material.tint"}};
        if (duplicateAliases)
            descriptor.parameterAliases.push_back(descriptor.parameterAliases[0]);
        return ShaderPtr(device->CreateShader(descriptor));
    }

    struct UploadRecord {
        BufferPtr              buffer;
        std::vector<std::byte> bytes;
        std::weak_ptr<int>     retention;
    };
    class ParameterContext final : public ShaderParameterUpdateContext {
    public:
        explicit ParameterContext(Device *device) : device(device)
        {
        }
        Device *GetDevice() const noexcept override
        {
            return device;
        }
        bool IsValid() const noexcept override
        {
            return valid;
        }
        bool UploadUniform(std::span<const std::byte> bytes, uint32_t alignment, ShaderUniformAllocation &out, std::string &error) override
        {
            if (!valid || !device || failUpload || !alignment || alignment & (alignment - 1)) {
                error = "Test uniform upload rejected";
                return false;
            }
            Buffer::Descriptor descriptor;
            descriptor.size   = (bytes.size() + alignment - 1) & ~uint64_t(alignment - 1);
            descriptor.usage  = BufferUsageFlagBit::UNIFORM;
            descriptor.memory = MemoryType::CPU_TO_GPU;
            auto buffer       = BufferPtr(device->CreateBuffer(descriptor));
            if (!buffer)
                return false;
            auto *mapped = buffer->Map();
            if (!mapped)
                return false;
            std::memset(mapped, 0, descriptor.size);
            std::memcpy(mapped, bytes.data(), bytes.size());
            buffer->UnMap();
            auto retention = std::make_shared<int>(1);
            out            = {buffer, 0, bytes.size(), retention};
            uploads.push_back({buffer, {bytes.begin(), bytes.end()}, retention});
            if (expireDuringUpload)
                valid = false;
            return true;
        }
        Device                   *device;
        bool                      valid = true, failUpload = false, expireDuringUpload = false;
        std::vector<UploadRecord> uploads;
    };

    BufferPtr MakeOutput(Device *device)
    {
        Buffer::Descriptor descriptor;
        descriptor.size   = 16;
        descriptor.usage  = BufferUsageFlagBit::STORAGE;
        descriptor.memory = MemoryType::GPU_TO_CPU;
        return BufferPtr(device->CreateBuffer(descriptor));
    }

    void StageRequired(DescriptorEncoder &encoder, Buffer *output, float gain = 2.f)
    {
        EXPECT_TRUE(encoder.WriteData(Name("Material.tint"), Vector4(.25f, .5f, .75f, 1.f)));
        EXPECT_TRUE(encoder.WriteData(Name("Material.gain"), gain));
        EXPECT_TRUE(encoder.WriteBuffer(Name("Output"), output, 0, 16));
    }

    class SnapshotDispatch final : public FrameCustomPassEncoder {
    public:
        CounterPtr<ComputePipeline> pipeline;
        ResourceGroupPtr            snapshot;
        mutable uint32_t            calls = 0;
        FrameOperationResult        Encode(FrameCustomPassContext &context) const noexcept override
        {
            ++calls;
            auto encoder = context.command.CreateComputeEncoder();
            if (!encoder)
                return FrameError{FrameErrorCode::COMPILE_FAILED, "Missing compute encoder"};
            encoder->BindPipeline(pipeline.Get());
            encoder->BindResourceGroup(0, snapshot.Get());
            encoder->Dispatch(1, 1, 1);
            return FrameOperationComplete{};
        }
    };
} // namespace

using NamedParameterMetalTest = AuroraMetalTest;

TEST_F(NamedParameterMetalTest, TypedUpdatesPublishDistinctSnapshotsAndRetainPreviousBytes)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto shader = MakeNamedShader(device);
    ASSERT_TRUE(shader);
    auto output = MakeOutput(device);
    ASSERT_TRUE(output);
    ResourceGroupPtr initial(device->CreateResourceGroup({shader.Get(), 0}));
    ASSERT_TRUE(initial);
    ParameterContext context(device);
    auto             first = initial->CreateEncoder(context);
    ASSERT_TRUE(first);
    StageRequired(*first, output.Get());
    EXPECT_FALSE(first->GetResult());
    ASSERT_TRUE(first->End()) << first->GetError();
    auto old = first->GetResult();
    ASSERT_TRUE(old);
    EXPECT_TRUE(old->IsParameterSnapshot());
    EXPECT_FALSE(old->CreateEncoder());
    std::unique_ptr<DescriptorBatch> batch(device->CreateDescriptorBatch());
    ASSERT_TRUE(batch);
    EXPECT_FALSE(batch->WriteBuffer(old.Get(), 0, output.Get(), 0, 16));
    EXPECT_FALSE(batch->Flush());
    EXPECT_NE(old.Get(), initial.Get());
    ASSERT_EQ(context.uploads.size(), 1u);
    auto oldBytes     = context.uploads[0].bytes;
    auto oldBuffer    = context.uploads[0].buffer;
    auto oldRetention = context.uploads[0].retention;
    EXPECT_FALSE(oldRetention.expired());
    auto second = old->CreateEncoder(context);
    ASSERT_TRUE(second);
    EXPECT_TRUE(second->WriteData(Name("Material.gain"), 3.f));
    ASSERT_TRUE(second->End()) << second->GetError();
    auto current = second->GetResult();
    ASSERT_TRUE(current);
    EXPECT_NE(current.Get(), old.Get());
    ASSERT_EQ(context.uploads.size(), 2u);
    EXPECT_NE(context.uploads[1].buffer.Get(), oldBuffer.Get());
    EXPECT_EQ(std::memcmp(context.uploads[1].bytes.data(), oldBytes.data(), 16), 0);
    float oldGain = 0, newGain = 0;
    std::memcpy(&oldGain, oldBytes.data() + 16, sizeof(float));
    std::memcpy(&newGain, context.uploads[1].bytes.data() + 16, sizeof(float));
    EXPECT_FLOAT_EQ(oldGain, 2.f);
    EXPECT_FLOAT_EQ(newGain, 3.f);
    auto *mapped = oldBuffer->Map();
    ASSERT_TRUE(mapped);
    EXPECT_EQ(std::memcmp(mapped, oldBytes.data(), oldBytes.size()), 0);
    oldBuffer->UnMap();
    EXPECT_FALSE(oldRetention.expired());
}

TEST_F(NamedParameterMetalTest, UnknownDuplicateAliasTypedAndOverlappingUpdatesNeverPublish)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto shader = MakeNamedShader(device);
    ASSERT_TRUE(shader);
    auto output = MakeOutput(device);
    ASSERT_TRUE(output);
    ResourceGroupPtr group(device->CreateResourceGroup({shader.Get(), 0}));
    ASSERT_TRUE(group);
    ParameterContext context(device);
    {
        auto update = group->CreateEncoder(context);
        EXPECT_FALSE(update->WriteData(Name("Missing"), 1.f));
        const auto first = update->GetError();
        EXPECT_FALSE(first.empty());
        EXPECT_FALSE(update->WriteData(Name("Material.gain"), uint32_t(1)));
        EXPECT_EQ(update->GetError(), first);
        EXPECT_FALSE(update->End());
        EXPECT_EQ(update->GetError(), first);
        EXPECT_FALSE(update->GetResult());
    }
    {
        auto update = group->CreateEncoder(context);
        EXPECT_TRUE(update->WriteData(Name("Material.tint"), Vector4(1.f)));
        EXPECT_FALSE(update->WriteData(Name("tintAlias"), Vector4(.5f)));
        EXPECT_FALSE(update->End());
        EXPECT_FALSE(update->GetResult());
    }
    {
        auto update = group->CreateEncoder(context);
        EXPECT_FALSE(update->WriteData(Name("Material.gain"), uint32_t(1)));
        EXPECT_FALSE(update->End());
        EXPECT_FALSE(update->GetResult());
    }
    {
        auto                            update = group->CreateEncoder(context);
        const auto                      block  = MaterialBlock();
        ShaderCpuSchema                 schema{block.size, block.alignment, block.members};
        const std::array<std::byte, 32> bytes{};
        EXPECT_TRUE(update->WriteBlock(Name("Material"), schema, bytes));
        EXPECT_FALSE(update->WriteData(Name("Material.tint"), Vector4(1.f)));
        EXPECT_FALSE(update->End());
        EXPECT_FALSE(update->GetResult());
    }
    EXPECT_TRUE(context.uploads.empty());
    EXPECT_FALSE(MakeNamedShader(device, false, true));
}

TEST_F(NamedParameterMetalTest, OwnerLeaseStaleHandleAndCancelRejectBeforeNativeCommit)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto shader = MakeNamedShader(device);
    auto newer  = MakeNamedShader(device, true);
    ASSERT_TRUE(shader);
    ASSERT_TRUE(newer);
    auto output = MakeOutput(device);
    ASSERT_TRUE(output);
    ResourceGroupPtr group(device->CreateResourceGroup({shader.Get(), 0}));
    ResourceGroupPtr newGroup(device->CreateResourceGroup({newer.Get(), 0}));
    ASSERT_TRUE(group);
    ASSERT_TRUE(newGroup);
    ParameterContext      context(device);
    ShaderParameterHandle handle;
    ASSERT_EQ(shader->GetParameterTable().Resolve(Name("Material.tint"), 0, handle), ShaderParameterLookup::READY);
    auto stale = newGroup->CreateEncoder(context);
    EXPECT_FALSE(stale->WriteData(handle, MakeShaderData(Vector4(1.f))));
    EXPECT_FALSE(stale->End());
    EXPECT_FALSE(stale->GetResult());

    Buffer unknown;
    auto   owner = group->CreateEncoder(context);
    EXPECT_FALSE(owner->WriteBuffer(Name("Output"), &unknown, 0, 16));
    EXPECT_FALSE(owner->End());
    EXPECT_FALSE(owner->GetResult());
    auto cancel = group->CreateEncoder(context);
    StageRequired(*cancel, output.Get());
    cancel->Cancel();
    EXPECT_FALSE(cancel->End());
    EXPECT_FALSE(cancel->GetResult());
    auto expired = group->CreateEncoder(context);
    StageRequired(*expired, output.Get());
    context.valid = false;
    EXPECT_FALSE(expired->End());
    EXPECT_FALSE(expired->GetResult());
    EXPECT_TRUE(context.uploads.empty());
    ParameterContext foreign(nullptr);
    auto             wrongContext = group->CreateEncoder(foreign);
    EXPECT_FALSE(wrongContext->WriteData(Name("Material.gain"), 1.f));
    EXPECT_FALSE(wrongContext->End());
    EXPECT_FALSE(wrongContext->GetResult());
}

TEST_F(NamedParameterMetalTest, KnownInactiveParametersAreValidatedAndDistinguishedFromUnknown)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto shader = MakeNamedShader(device);
    ASSERT_TRUE(shader);
    auto output = MakeOutput(device);
    ASSERT_TRUE(output);
    ResourceGroupPtr group(device->CreateResourceGroup({shader.Get(), 0}));
    ASSERT_TRUE(group);
    Image::Descriptor descriptor;
    descriptor.format = PixelFormat::RGBA8_UNORM;
    descriptor.extent = {1, 1, 1};
    descriptor.usage  = ImageUsageFlagBit::SAMPLED;
    ImagePtr   image(device->CreateImage(descriptor));
    SamplerPtr sampler(device->CreateSampler({}));
    ASSERT_TRUE(image);
    ASSERT_TRUE(sampler);
    ParameterContext context(device);
    auto             update = group->CreateEncoder(context);
    EXPECT_EQ(update->WriteData(Name("Optional.value"), 1.f).status, ShaderParameterWriteStatus::INACTIVE);
    EXPECT_EQ(update->WriteImage(Name("UnusedTexture"), image.Get(), ImageLayout::SHADER_READ_ONLY).status, ShaderParameterWriteStatus::INACTIVE);
    EXPECT_EQ(update->WriteSampler(Name("UnusedSampler"), sampler.Get()).status, ShaderParameterWriteStatus::INACTIVE);
    EXPECT_EQ(update->WriteBuffer(Name("UnusedBuffer"), output.Get(), 0, 16).status, ShaderParameterWriteStatus::INACTIVE);
    StageRequired(*update, output.Get());
    ASSERT_TRUE(update->End()) << update->GetError();
    ASSERT_EQ(context.uploads.size(), 1u);
    EXPECT_TRUE(update->GetResult());
    auto wrongType = group->CreateEncoder(context);
    EXPECT_EQ(wrongType->WriteData(Name("Optional.value"), uint32_t(1)).status, ShaderParameterWriteStatus::FAILED);
    EXPECT_FALSE(wrongType->End());
    EXPECT_FALSE(wrongType->GetResult());
    descriptor.format = PixelFormat::R8_UINT;
    ImagePtr unsignedImage(device->CreateImage(descriptor));
    ASSERT_TRUE(unsignedImage);
    auto wrongImage = group->CreateEncoder(context);
    EXPECT_EQ(wrongImage->WriteImage(Name("UnusedTexture"), unsignedImage.Get(), ImageLayout::SHADER_READ_ONLY).status,
              ShaderParameterWriteStatus::FAILED);
    EXPECT_FALSE(wrongImage->End());
    EXPECT_FALSE(wrongImage->GetResult());
    Buffer::Descriptor bufferDescriptor;
    bufferDescriptor.size  = 16;
    bufferDescriptor.usage = BufferUsageFlagBit::UNIFORM;
    BufferPtr uniformOnly(device->CreateBuffer(bufferDescriptor));
    ASSERT_TRUE(uniformOnly);
    auto wrongBuffer = group->CreateEncoder(context);
    EXPECT_EQ(wrongBuffer->WriteBuffer(Name("UnusedBuffer"), uniformOnly.Get(), 0, 16).status, ShaderParameterWriteStatus::FAILED);
    EXPECT_FALSE(wrongBuffer->End());
    EXPECT_FALSE(wrongBuffer->GetResult());
}

TEST_F(NamedParameterMetalTest, FailedUniformUploadCannotReplaceAnExistingSnapshot)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto shader = MakeNamedShader(device);
    ASSERT_TRUE(shader);
    auto output = MakeOutput(device);
    ASSERT_TRUE(output);
    ResourceGroupPtr group(device->CreateResourceGroup({shader.Get(), 0}));
    ASSERT_TRUE(group);
    ParameterContext context(device);
    auto             first = group->CreateEncoder(context);
    StageRequired(*first, output.Get());
    ASSERT_TRUE(first->End()) << first->GetError();
    auto snapshot = first->GetResult();
    ASSERT_TRUE(snapshot);
    const auto bytes   = context.uploads[0].bytes;
    context.failUpload = true;
    auto failed        = snapshot->CreateEncoder(context);
    EXPECT_TRUE(failed->WriteData(Name("Material.gain"), 4.f));
    EXPECT_FALSE(failed->End());
    EXPECT_FALSE(failed->GetResult());
    ASSERT_EQ(context.uploads.size(), 1u);
    EXPECT_EQ(context.uploads[0].bytes, bytes);
    context.failUpload         = false;
    context.expireDuringUpload = true;
    auto expired               = snapshot->CreateEncoder(context);
    EXPECT_TRUE(expired->WriteData(Name("Material.gain"), 4.f));
    EXPECT_FALSE(expired->End());
    EXPECT_FALSE(expired->GetResult());
    EXPECT_EQ(context.uploads[0].bytes, bytes);
}

TEST_F(NamedParameterMetalTest, CommittedSnapshotDispatchesThroughItsPhysicalMetalSlots)
{
    auto *device = GetDevice();
    ASSERT_TRUE(device);
    auto shader = MakeNamedShader(device);
    ASSERT_TRUE(shader);
    auto output = MakeOutput(device);
    ASSERT_TRUE(output);
    ResourceGroupPtr group(device->CreateResourceGroup({shader.Get(), 0}));
    ASSERT_TRUE(group);
    ParameterContext parameters(device);
    auto             update = group->CreateEncoder(parameters);
    StageRequired(*update, output.Get());
    ASSERT_TRUE(update->End()) << update->GetError();
    auto operation      = std::make_shared<SnapshotDispatch>();
    operation->snapshot = update->GetResult();
    operation->pipeline = CounterPtr<ComputePipeline>(device->CreatePipelineState(ComputePipeline::Descriptor{shader.Get()}));
    ASSERT_TRUE(operation->snapshot);
    ASSERT_TRUE(operation->pipeline);
    FrameGraphData data;
    data.buffers.push_back({parameters.uploads[0].buffer, 256, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true});
    data.buffers.push_back({output, 16, CopyBufferState::COMMON, CopyBufferState::HOST_READ, false});
    FrameCustomPass pass;
    pass.encoder = operation;
    pass.buffers = {{0, 3, {{1, 0}, 0, 32}, 0, FrameAccessMode::READ, FrameBufferBindingKind::UNIFORM},
                    {0,
                     0,
                     {{1, 1}, 0, 16},
                     0,
                     FrameAccessMode::WRITE,
                     FrameBufferBindingKind::STORAGE,
                     ShaderResourceAccess::READ_WRITE,
                     ShaderBufferKind::STRUCTURED,
                     16}};
    data.passes.emplace_back(std::move(pass));
    data.exports.push_back({{1, 1}, 0, 16});
    auto                                graph = std::make_shared<const FrozenFrameGraph>(1, std::move(data));
    std::unique_ptr<DeviceFrameContext> context(device->CreateFrameContext({1, 1, 1}));
    ASSERT_TRUE(context);
    const auto begun = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begun));
    auto compiled = context->Compile(std::get<DeviceFrameLease>(begun), graph);
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto       plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    const auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = context->TryCollect(std::get<EncodingTicket>(dispatched));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
    bool complete = false;
    for (uint32_t i = 0; i < 2000 && !complete; ++i) {
        std::array<FrameCompletion, 1> completion;
        if (context->PollCompleted(completion))
            complete = completion[0].status == FrameCompletionStatus::COMPLETE;
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(complete);
    EXPECT_EQ(operation->calls, 1u);
    auto *mapped = output->Map();
    ASSERT_TRUE(mapped);
    std::array<float, 4> actual;
    std::memcpy(actual.data(), mapped, sizeof(actual));
    output->UnMap();
    const std::array<float, 4> expected{.5f, 1.f, 1.5f, 2.f};
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(actual[i], expected[i]);
}
#endif
