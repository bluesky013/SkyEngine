//
// Aurora ResourceGroup tests (Vulkan + DX12).
// ResourceGroup is created from {shader, set}; its layout is derived from the
// shader reflection, not from a hand-written ResourceGroupLayout.
//

#include "AuroraTestHelper.h"
#include "ResourceGroupShaderFixtures.h"
#include "ShaderArtifactTestHelper.h"

#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/Shader.h>
#include <aurora/rhi/ShaderReflection.h>
#include <core/archive/BinaryData.h>
#include <cstring>
#include <iterator>

#if defined(SKY_PLATFORM_WINDOWS)
    #include <core/util/DynamicModule.h>
    #include <d3d12shader.h>
    #include <dxcapi.h>
    #include <wrl/client.h>
#endif

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    // Minimal compute SPIR-V: layout(local_size_x=1) in; void main() {}
    static const uint32_t SPIRV_CS[] = {
        0x07230203, 0x00010000, 0x00000000, 0x00000005, 0x00000000, 0x00020011, 0x00000001, 0x0003000E, 0x00000000,
        0x00000001, 0x0005000F, 0x00000005, 0x00000001, 0x6E69616D, 0x00000000, 0x00060010, 0x00000001, 0x00000011,
        0x00000001, 0x00000001, 0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00050036,
        0x00000002, 0x00000001, 0x00000000, 0x00000003, 0x000200F8, 0x00000004, 0x000100FD, 0x00010038,
    };

    ShaderReflection MakeUboReflection(uint32_t set)
    {
        ShaderReflection reflection;
        ShaderResource   resource;
        resource.name             = "Ubo";
        resource.set              = set;
        resource.binding          = 0;
        resource.type             = ShaderResourceType::UNIFORM_BUFFER;
        resource.stages           = ShaderStageFlagBit::CS;
        resource.declarationOwner = "rhi-test/resource-group";
        reflection.resources.push_back(resource);
        reflection.declarations.push_back(resource);
        ShaderBlockLayout block;
        block.name       = "Ubo";
        block.structName = "UboData";
        block.set        = set;
        block.size       = 16;
        block.alignment  = 16;
        ShaderBlockMember member;
        member.name       = "color";
        member.size       = 16;
        member.scalarType = ShaderScalarType::FLOAT;
        member.kind       = ShaderTypeKind::VECTOR;
        member.cols       = 4;
        member.alignment  = 16;
        block.members.push_back(member);
        reflection.blocks.push_back(block);
        reflection.declaredBlocks.push_back(block);
        reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
        return reflection;
    }

    ShaderReflection MakeDynamicUboReflection(uint32_t set)
    {
        auto reflection              = MakeUboReflection(set);
        reflection.resources[0].type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        return reflection;
    }

    ShaderReflection MakeMixedReflection(uint32_t set)
    {
        auto           reflection = MakeUboReflection(set);
        ShaderResource image;
        image.name             = "Tex";
        image.set              = set;
        image.binding          = 1;
        image.type             = ShaderResourceType::SAMPLED_IMAGE;
        image.texture          = {ShaderTextureDimension::D2, ShaderScalarType::FLOAT, 4, false, false};
        image.stages           = ShaderStageFlagBit::CS;
        image.declarationOwner = "rhi-test/resource-group";
        reflection.resources.push_back(image);
        ShaderResource sampler;
        sampler.name             = "Smp";
        sampler.set              = set;
        sampler.binding          = 2;
        sampler.type             = ShaderResourceType::SAMPLER;
        sampler.sampler          = ShaderSamplerType::REGULAR;
        sampler.stages           = ShaderStageFlagBit::CS;
        sampler.declarationOwner = "rhi-test/resource-group";
        reflection.resources.push_back(sampler);
        reflection.declarations = reflection.resources;
        return reflection;
    }

#if defined(SKY_PLATFORM_WINDOWS)
    bool HasDxcCompiler()
    {
        DynamicModule module("dxcompiler");
        return module.Load() && module.GetAddress<DxcCreateInstanceProc>("DxcCreateInstance");
    }

    std::vector<uint8_t> CompileDxil(const ShaderReflection &reflection)
    {
        const char *source = reflection.resources.empty() ? "[numthreads(1,1,1)] void main() {}" : R"HLSL(
struct UboData { float4 color; };
ConstantBuffer<UboData> Ubo : register(b5, space3);
[numthreads(1,1,1)] void main() { if (Ubo.color.x < 0) DeviceMemoryBarrier(); }
)HLSL";
        if (reflection.blocks.size() == 1 && reflection.blocks[0].size == 192)
            source = R"HLSL(
struct UboData { float4 color[12]; };
ConstantBuffer<UboData> Ubo : register(b5, space3);
[numthreads(1,1,1)] void main() { if ((Ubo.color[0] + Ubo.color[11]).x < 0) DeviceMemoryBarrier(); }
)HLSL";
        if (reflection.resources.size() == 3)
            source = R"HLSL(
struct UboData { float4 color; };
ConstantBuffer<UboData> Ubo : register(b5, space3);
Texture2D<float4> Tex : register(t7, space3);
SamplerState Smp : register(s2, space3);
[numthreads(1,1,1)] void main() {
    if ((Ubo.color + Tex.SampleLevel(Smp, float2(0.5,0.5), 0)).x < 0) DeviceMemoryBarrier();
}
)HLSL";
        if (reflection.resources.size() == 4)
            source = R"HLSL(
struct Element { float3 value; };
ByteAddressBuffer ReadRaw : register(t8, space3);
RWByteAddressBuffer WriteRaw : register(u9, space3);
StructuredBuffer<Element> ReadStructured : register(t10, space3);
RWStructuredBuffer<Element> WriteStructured : register(u11, space3);
[numthreads(1,1,1)] void main() {
    WriteRaw.Store(0, ReadRaw.Load(0));
    WriteStructured[0] = ReadStructured[0];
}
)HLSL";
        DynamicModule module("dxcompiler");
        if (!module.Load())
            return {};
        auto create = module.GetAddress<DxcCreateInstanceProc>("DxcCreateInstance");
        if (!create)
            return {};
        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
        Microsoft::WRL::ComPtr<IDxcUtils>     utils;
        if (FAILED(create(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf()))) ||
            FAILED(create(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf())))) {
            ADD_FAILURE() << "Cannot initialize DXC fixture compiler";
            return {};
        }
        DxcBuffer                          input{source, std::strlen(source), DXC_CP_UTF8};
        const wchar_t                     *arguments[] = {L"-E", L"main", L"-T", L"cs_6_5", L"-Od"};
        Microsoft::WRL::ComPtr<IDxcResult> result;
        const auto                         compiled =
            compiler->Compile(&input, arguments, static_cast<UINT32>(std::size(arguments)), nullptr, IID_PPV_ARGS(result.GetAddressOf()));
        HRESULT status = E_FAIL;
        if (FAILED(compiled) || !result || FAILED(result->GetStatus(&status)) || FAILED(status)) {
            Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
            if (result)
                result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);
            ADD_FAILURE() << (errors ? errors->GetStringPointer() : "DXC fixture compilation failed");
            return {};
        }
        Microsoft::WRL::ComPtr<IDxcBlob> object, metadata;
        if (FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(object.GetAddressOf()), nullptr)) || !object ||
            FAILED(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(metadata.GetAddressOf()), nullptr)) || !metadata)
            return {};
        DxcBuffer                                      nativeMetadata{metadata->GetBufferPointer(), metadata->GetBufferSize(), 0};
        Microsoft::WRL::ComPtr<ID3D12ShaderReflection> native;
        if (FAILED(utils->CreateReflection(&nativeMetadata, IID_PPV_ARGS(native.GetAddressOf()))))
            return {};
        for (const auto &resource : reflection.resources) {
            D3D12_SHADER_INPUT_BIND_DESC binding{};
            const auto                   mapping = std::find_if(reflection.nativeBindings.begin(), reflection.nativeBindings.end(),
                                                                [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
            if (FAILED(native->GetResourceBindingDescByName(resource.name.c_str(), &binding)) || mapping == reflection.nativeBindings.end() ||
                binding.BindPoint != mapping->index || binding.Space != mapping->space || binding.BindCount != mapping->count) {
                ADD_FAILURE() << "DXC fixture's real native address disagrees with its explicit artifact map: " << resource.name;
                return {};
            }
            const auto expected = resource.type == ShaderResourceType::UNIFORM_BUFFER  ? D3D_SIT_CBUFFER
                                  : resource.type == ShaderResourceType::SAMPLED_IMAGE ? D3D_SIT_TEXTURE
                                  : resource.type == ShaderResourceType::SAMPLER       ? D3D_SIT_SAMPLER
                                  : resource.bufferKind == ShaderBufferKind::RAW
                                      ? (resource.access == ShaderResourceAccess::READ_ONLY ? D3D_SIT_BYTEADDRESS : D3D_SIT_UAV_RWBYTEADDRESS)
                                      : (resource.access == ShaderResourceAccess::READ_ONLY ? D3D_SIT_STRUCTURED : D3D_SIT_UAV_RWSTRUCTURED);
            if (binding.Type != expected) {
                ADD_FAILURE() << "DXC fixture's real native register class disagrees with its artifact: " << resource.name;
                return {};
            }
        }
        for (const auto &block : reflection.blocks) {
            D3D12_SHADER_BUFFER_DESC desc{};
            auto                    *buffer = native->GetConstantBufferByName(block.name.c_str());
            if (!buffer || FAILED(buffer->GetDesc(&desc)) || desc.Size != block.size) {
                ADD_FAILURE() << "DXC fixture's real uniform block size disagrees with its artifact: " << block.name;
                return {};
            }
        }
        const auto *bytes = static_cast<const uint8_t *>(object->GetBufferPointer());
        return {bytes, bytes + object->GetBufferSize()};
    }
#endif

    Shader *MakeComputeShader(Device *device, ShaderReflection reflection)
    {
        const auto                       target = device->GetAPI() == API::DX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;
        std::vector<ShaderBindingPolicy> policies;
        reflection.nativeBindings.clear();
        for (auto &resource : reflection.resources) {
            if (resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || resource.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                policies.push_back({resource.set, resource.binding, resource.type});
                resource.type = resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ? ShaderResourceType::UNIFORM_BUFFER
                                                                                            : ShaderResourceType::STORAGE_BUFFER;
            }
            ShaderNativeBinding native{resource.set, resource.binding, ShaderStageFlagBit::CS};
            native.space = target == ShaderTarget::DXIL ? 3 : 0;
            if (target == ShaderTarget::SPIRV) {
                native.category = ShaderNativeCategory::DESCRIPTOR;
                native.index = resource.type == ShaderResourceType::UNIFORM_BUFFER ? 7 : resource.type == ShaderResourceType::SAMPLED_IMAGE ? 9 : 11;
            } else {
                switch (resource.type) {
                case ShaderResourceType::UNIFORM_BUFFER:
                    native.category = ShaderNativeCategory::CBV;
                    native.index    = 5;
                    break;
                case ShaderResourceType::SAMPLED_IMAGE:
                    native.category = ShaderNativeCategory::SRV;
                    native.index    = 7;
                    break;
                case ShaderResourceType::SAMPLER:
                    native.category = ShaderNativeCategory::SAMPLER;
                    native.index    = 2;
                    break;
                default:
                    native.category = resource.access == ShaderResourceAccess::READ_ONLY ? ShaderNativeCategory::SRV : ShaderNativeCategory::UAV;
                    native.index    = resource.name == "ReadRaw" ? 8 : resource.name == "WriteRaw" ? 9 : resource.name == "ReadStructured" ? 10 : 11;
                    break;
                }
            }
            reflection.nativeBindings.push_back(native);
        }
        std::vector<uint8_t> code;
        if (target == ShaderTarget::SPIRV) {
            const auto *words = reflection.resources.empty() ? SPIRV_CS : reflection.resources.size() == 3 ? kGroupMixedSpirv : kGroupUboSpirv;
            const auto  size  = reflection.resources.empty()       ? sizeof(SPIRV_CS)
                                : reflection.resources.size() == 3 ? sizeof(kGroupMixedSpirv)
                                                                   : sizeof(kGroupUboSpirv);
            const auto *bytes = reinterpret_cast<const uint8_t *>(words);
            code.assign(bytes, bytes + size);
        }
#if defined(SKY_PLATFORM_WINDOWS)
        else
            code = CompileDxil(reflection);
#endif
        auto artifact = MakeTestShaderArtifact(target, ShaderStageFlagBit::CS, code, "main", std::move(reflection));
        if (!artifact)
            return nullptr;
        ShaderFunction::Descriptor function{};
        function.stage    = ShaderStageFlagBit::CS;
        function.artifact = std::move(artifact);
        auto cs           = ShaderFunctionPtr(device->CreateShaderFunction(function));
        if (!cs)
            return nullptr;
        Shader::Descriptor shader{};
        shader.cs              = cs.Get();
        shader.bindingPolicies = std::move(policies);
        return device->CreateShader(shader);
    }

} // namespace

using ResourceGroupTestVulkan = AuroraVulkanTest;

TEST_F(ResourceGroupTestVulkan, CreateEmptyGroup)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ShaderReflection refl{}; // empty reflection: shader creation is valid
    auto             shader = CounterPtr<Shader>(MakeComputeShader(device, refl));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 0;
    auto group = ResourceGroupPtr(device->CreateResourceGroup(gd));
    ASSERT_TRUE(group);
    ASSERT_TRUE(group->CreateEncoder());
}

TEST_F(ResourceGroupTestVulkan, GroupRequiresShader)
{
    auto                     *device = GetDevice();
    ResourceGroup::Descriptor gd{};
    gd.shader   = nullptr;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestVulkan, UpdateUniformBuffer)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(0)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 0;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub   = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    EXPECT_TRUE(encoder->WriteBuffer(0, ub.Get(), 0, 256));
    EXPECT_TRUE(encoder->End());
}

TEST_F(ResourceGroupTestVulkan, EncoderBatchWrite)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeMixedReflection(0)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 0;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub   = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    Image::Descriptor id{};
    id.imageType   = ImageType::IMAGE_2D;
    id.format      = PixelFormat::RGBA8_UNORM;
    id.extent      = {16, 16, 1};
    id.mipLevels   = 1;
    id.arrayLayers = 1;
    id.samples     = SampleCount::X1;
    id.usage       = ImageUsageFlagBit::SAMPLED;
    id.memory      = MemoryType::GPU_ONLY;
    auto img       = CounterPtr<Image>(device->CreateImage(id));
    ASSERT_NE(img.Get(), nullptr);

    Sampler::Descriptor sd{};
    sd.magFilter    = Filter::LINEAR;
    sd.minFilter    = Filter::LINEAR;
    sd.mipmapMode   = MipFilter::LINEAR;
    sd.addressModeU = WrapMode::REPEAT;
    sd.addressModeV = WrapMode::REPEAT;
    sd.addressModeW = WrapMode::REPEAT;
    auto smp        = CounterPtr<Sampler>(device->CreateSampler(sd));
    ASSERT_NE(smp.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    EXPECT_TRUE(encoder->WriteBuffer(0, ub.Get(), 0, 256));
    EXPECT_TRUE(encoder->WriteImage(1, img.Get(), ImageLayout::SHADER_READ_ONLY));
    EXPECT_TRUE(encoder->WriteSampler(2, smp.Get()));
    EXPECT_TRUE(encoder->End());
}

TEST_F(ResourceGroupTestVulkan, DescriptorBatchCrossSet)
{
    auto *device = GetDevice();

    auto shader0 = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(0)));
    auto shader1 = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(1)));
    ASSERT_NE(shader0.Get(), nullptr);
    ASSERT_NE(shader1.Get(), nullptr);

    ResourceGroup::Descriptor gd0{};
    gd0.shader = shader0.Get();
    gd0.set    = 0;
    ResourceGroup::Descriptor gd1{};
    gd1.shader  = shader1.Get();
    gd1.set     = 1;
    auto group0 = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd0));
    auto group1 = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd1));
    ASSERT_NE(group0.Get(), nullptr);
    ASSERT_NE(group1.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub   = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto *batch = device->CreateDescriptorBatch();
    ASSERT_NE(batch, nullptr);

    EXPECT_TRUE(batch->WriteBuffer(group0.Get(), 0, ub.Get(), 0, 256));
    EXPECT_TRUE(batch->WriteBuffer(group1.Get(), 0, ub.Get(), 0, 256));
    EXPECT_TRUE(batch->Flush());
    batch->Reset();

    delete batch;
}

TEST_F(ResourceGroupTestVulkan, SetIndexHole)
{
    auto *device = GetDevice();

    // only set 2 has a resource; set 0/1 are absent (hole)
    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(2)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 2;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

#if defined(SKY_PLATFORM_WINDOWS)
class ResourceGroupTestD3D12 : public AuroraD3D12Test {
protected:
    void SetUp() override
    {
        if (!HasDxcCompiler())
            GTEST_SKIP() << "dxcompiler.dll is required to compile genuine DXIL resource fixtures";
    }
};

TEST_F(ResourceGroupTestD3D12, CreateEmptyGroup)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    ShaderReflection refl{};
    auto             shader = CounterPtr<Shader>(MakeComputeShader(device, refl));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 0;
    auto group = ResourceGroupPtr(device->CreateResourceGroup(gd));
    ASSERT_TRUE(group);
    ASSERT_TRUE(group->CreateEncoder());
}

TEST_F(ResourceGroupTestD3D12, GroupRequiresShader)
{
    auto                     *device = GetDevice();
    ResourceGroup::Descriptor gd{};
    gd.shader   = nullptr;
    auto *group = device->CreateResourceGroup(gd);
    EXPECT_EQ(group, nullptr);
}

TEST_F(ResourceGroupTestD3D12, UpdateUniformBuffer)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(0)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 0;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 256;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub   = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    EXPECT_TRUE(encoder->WriteBuffer(0, ub.Get(), 0, 256));
    EXPECT_TRUE(encoder->End());
}

TEST_F(ResourceGroupTestD3D12, SetIndexHole)
{
    auto *device = GetDevice();

    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeUboReflection(2)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 2;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);
}

TEST_F(ResourceGroupTestD3D12, UpdateDynamicUniformBuffer)
{
    auto *device = GetDevice();

    // root CBV is derived from the reflection; verify it serializes + updates
    auto shader = CounterPtr<Shader>(MakeComputeShader(device, MakeDynamicUboReflection(2)));
    ASSERT_NE(shader.Get(), nullptr);

    ResourceGroup::Descriptor gd{};
    gd.shader  = shader.Get();
    gd.set     = 2;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(gd));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bd{};
    bd.size   = 4096;
    bd.usage  = BufferUsageFlagBit::UNIFORM;
    bd.memory = MemoryType::CPU_TO_GPU;
    auto ub   = CounterPtr<Buffer>(device->CreateBuffer(bd));
    ASSERT_NE(ub.Get(), nullptr);

    auto encoder = group->CreateEncoder();
    EXPECT_TRUE(encoder->WriteBuffer(0, ub.Get(), 0, 256)); // dynamic binding: recorded, no descriptor write
    EXPECT_TRUE(encoder->End());
}

TEST_F(ResourceGroupTestD3D12, RejectsUniformLogicalRangesSmallerThanReflectedBlock)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Buffer::Descriptor descriptor{};
    descriptor.size   = 256;
    descriptor.usage  = BufferUsageFlagBit::UNIFORM;
    descriptor.memory = MemoryType::CPU_TO_GPU;
    auto buffer       = BufferPtr(device->CreateBuffer(descriptor));
    ASSERT_TRUE(buffer);

    for (const bool dynamic : {false, true}) {
        SCOPED_TRACE(dynamic ? "dynamic UBO" : "static UBO");
        auto  reflection          = dynamic ? MakeDynamicUboReflection(2) : MakeUboReflection(2);
        auto &block               = reflection.blocks[0];
        block.size                = 192;
        auto &member              = block.members[0];
        member.kind               = ShaderTypeKind::ARRAY;
        member.size               = 192;
        member.arrayCount         = 12;
        member.arrayStride        = 16;
        reflection.declaredBlocks = reflection.blocks;
        auto shader               = ShaderPtr(MakeComputeShader(device, std::move(reflection)));
        ASSERT_TRUE(shader);
        auto group = ResourceGroupPtr(device->CreateResourceGroup({shader.Get(), 2}));
        ASSERT_TRUE(group);

        auto valid = group->CreateEncoder();
        ASSERT_TRUE(valid);
        EXPECT_TRUE(valid->WriteBuffer(0, buffer.Get(), 0, 192));
        EXPECT_TRUE(valid->End());

        auto undersized = group->CreateEncoder();
        ASSERT_TRUE(undersized);
        EXPECT_FALSE(undersized->WriteBuffer(0, buffer.Get(), 0, 16));
        EXPECT_FALSE(undersized->WriteBuffer(0, buffer.Get(), 0, 192));
        EXPECT_FALSE(undersized->End());

        auto next = group->CreateEncoder();
        ASSERT_TRUE(next);
        EXPECT_TRUE(next->WriteBuffer(0, buffer.Get(), 0, 192));
        EXPECT_TRUE(next->End());
    }
}

namespace {
    ShaderReflection MakeStorageReflection(bool dynamic = false)
    {
        ShaderReflection reflection;
        const char      *names[]    = {"ReadRaw", "WriteRaw", "ReadStructured", "WriteStructured"};
        const uint32_t   bindings[] = {3, 7, 9, 13};
        for (uint32_t i = 0; i < 4; ++i) {
            ShaderResource resource;
            resource.name             = names[i];
            resource.set              = 1;
            resource.binding          = bindings[i];
            resource.type             = ShaderResourceType::STORAGE_BUFFER;
            resource.access           = i % 2 ? ShaderResourceAccess::READ_WRITE : ShaderResourceAccess::READ_ONLY;
            resource.bufferKind       = i < 2 ? ShaderBufferKind::RAW : ShaderBufferKind::STRUCTURED;
            resource.elementStride    = i < 2 ? 0 : 12;
            resource.stages           = ShaderStageFlagBit::CS;
            resource.declarationOwner = "rhi-test/resource-group";
            reflection.declarations.push_back(resource);
            if (dynamic)
                resource.type = ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
            reflection.resources.push_back(resource);
        }
        reflection.threadGroupSize[0] = reflection.threadGroupSize[1] = reflection.threadGroupSize[2] = 1;
        return reflection;
    }

    class UnknownOwnerBuffer final : public Buffer {
    public:
        uint8_t *Map() override
        {
            return nullptr;
        }
        void UnMap() override
        {
        }
        uint64_t GetSize() const override
        {
            return 1536;
        }
    };
} // namespace

TEST_F(ResourceGroupTestD3D12, StorageViewsPreserveAccessKindStrideAndExactRange)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    auto shader = ShaderPtr(MakeComputeShader(device, MakeStorageReflection()));
    ASSERT_TRUE(shader);
    auto group = ResourceGroupPtr(device->CreateResourceGroup({shader.Get(), 1}));
    ASSERT_TRUE(group);
    Buffer::Descriptor descriptor;
    descriptor.size   = 1536;
    descriptor.usage  = BufferUsageFlagBit::STORAGE;
    descriptor.memory = MemoryType::GPU_ONLY;
    auto buffer       = BufferPtr(device->CreateBuffer(descriptor));
    ASSERT_TRUE(buffer);
    auto encoder = group->CreateEncoder();
    ASSERT_TRUE(encoder);
    EXPECT_TRUE(encoder->WriteBuffer(3, buffer.Get(), 16, 64));
    EXPECT_TRUE(encoder->WriteBuffer(7, buffer.Get(), 32, 128));
    EXPECT_TRUE(encoder->WriteBuffer(9, buffer.Get(), 12, 36));
    EXPECT_TRUE(encoder->WriteBuffer(13, buffer.Get(), 24, 72));
    EXPECT_TRUE(encoder->End());

    auto invalidRaw = group->CreateEncoder();
    EXPECT_FALSE(invalidRaw->WriteBuffer(3, buffer.Get(), 4, 64));
    EXPECT_FALSE(invalidRaw->WriteBuffer(3, buffer.Get(), 16, 64));
    EXPECT_FALSE(invalidRaw->End());
    auto invalidStructured = group->CreateEncoder();
    EXPECT_FALSE(invalidStructured->WriteBuffer(9, buffer.Get(), 12, 28));
    EXPECT_FALSE(invalidStructured->End());
    auto invalidRange = group->CreateEncoder();
    EXPECT_FALSE(invalidRange->WriteBuffer(13, buffer.Get(), 1512, 36));
    EXPECT_FALSE(invalidRange->End());
    auto invalidArray = group->CreateEncoder();
    EXPECT_FALSE(invalidArray->WriteBuffer(3, buffer.Get(), 16, 64, 1));
    EXPECT_FALSE(invalidArray->End());
}

TEST_F(ResourceGroupTestD3D12, DynamicStorageCreatesNativeSrvAndUavRoots)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    auto shader = ShaderPtr(MakeComputeShader(device, MakeStorageReflection(true)));
    ASSERT_TRUE(shader);
    auto group = ResourceGroupPtr(device->CreateResourceGroup({shader.Get(), 1}));
    ASSERT_TRUE(group);
    Buffer::Descriptor descriptor;
    descriptor.size   = 1536;
    descriptor.usage  = BufferUsageFlagBit::STORAGE;
    descriptor.memory = MemoryType::GPU_ONLY;
    auto buffer       = BufferPtr(device->CreateBuffer(descriptor));
    ASSERT_TRUE(buffer);
    auto encoder = group->CreateEncoder();
    ASSERT_TRUE(encoder);
    EXPECT_TRUE(encoder->WriteBuffer(3, buffer.Get(), 16, 64));
    EXPECT_TRUE(encoder->WriteBuffer(7, buffer.Get(), 32, 128));
    EXPECT_TRUE(encoder->WriteBuffer(9, buffer.Get(), 12, 36));
    EXPECT_TRUE(encoder->WriteBuffer(13, buffer.Get(), 24, 72));
    EXPECT_TRUE(encoder->End());
}

TEST_F(ResourceGroupTestD3D12, StorageRejectsUnknownOwnersUsageAndLatchesBatchFailure)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    auto shader = ShaderPtr(MakeComputeShader(device, MakeStorageReflection()));
    ASSERT_TRUE(shader);
    auto group = ResourceGroupPtr(device->CreateResourceGroup({shader.Get(), 1}));
    ASSERT_TRUE(group);
    UnknownOwnerBuffer unknown;
    auto               owner = group->CreateEncoder();
    EXPECT_FALSE(owner->WriteBuffer(3, &unknown, 16, 64));
    EXPECT_FALSE(owner->End());

    Buffer::Descriptor descriptor;
    descriptor.size   = 1536;
    descriptor.usage  = BufferUsageFlagBit::UNIFORM;
    descriptor.memory = MemoryType::CPU_TO_GPU;
    auto buffer       = BufferPtr(device->CreateBuffer(descriptor));
    ASSERT_TRUE(buffer);
    auto usage = group->CreateEncoder();
    EXPECT_FALSE(usage->WriteBuffer(3, buffer.Get(), 16, 64));
    EXPECT_FALSE(usage->End());

    std::unique_ptr<DescriptorBatch> batch(device->CreateDescriptorBatch());
    ASSERT_TRUE(batch);
    EXPECT_FALSE(batch->WriteBuffer(group.Get(), 3, &unknown, 16, 64));
    EXPECT_FALSE(batch->Flush());
    batch->Reset();
    EXPECT_TRUE(batch->Flush());
}
#endif
