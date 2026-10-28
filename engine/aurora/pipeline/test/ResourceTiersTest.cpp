//
// Resource binding and shared declaration reflection validation.
//

#include "AuroraTestHelper.h"
#include "GlobalBlock.h"

#include <aurora/pipeline/ReflectionValidation.h>
#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    Shader *MakeDynamicBatchShader(Device *device, uint32_t &set, uint32_t &binding, uint32_t &blockSize)
    {
        const char *src = R"(
#include <aurora/pipeline/SceneBatch.slangh>
[AuroraResource(1, 0, "test/ResourceTiers")]
RWStructuredBuffer<float4> values : register(u0, space1);
[shader("compute")]
[numthreads(1, 1, 1)]
void mainCS() { values[0] = gSceneBatch.world[0]; }
)";

        ShaderFileSystem files;
        files.AddSearchPath(AURORA_SHADER_ASSET_DIR);
        ShaderCompilerSlang compiler;
        ShaderCompileDesc   d{};
        d.fileSystem = &files;
        d.source     = src;
        d.entry      = "mainCS";
        d.stage      = ShaderStageFlagBit::CS;
        d.target     = ShaderTarget::SPIRV;

        ShaderCompileResult r{};
        if (!compiler.Compile(d, r)) {
            return nullptr;
        }

        set       = 0;
        binding   = 0;
        blockSize = 0;
        for (const auto &res : r.reflection.resources) {
            if (res.type == ShaderResourceType::UNIFORM_BUFFER) {
                set     = res.set;
                binding = res.binding;
            }
        }
        for (const auto &block : r.reflection.blocks) {
            if (block.set == set && block.binding == binding) {
                blockSize = block.size;
            }
        }
        if (blockSize == 0)
            return nullptr;

        ShaderFunction::Descriptor fnDesc{};
        fnDesc.stage    = ShaderStageFlagBit::CS;
        fnDesc.artifact = std::make_shared<const ShaderStageArtifact>(r);
        auto *cs        = device->CreateShaderFunction(fnDesc);
        if (cs == nullptr) {
            return nullptr;
        }

        Shader::Descriptor shaderDesc = {};
        shaderDesc.cs                 = cs;
        shaderDesc.bindingPolicies.push_back({set, binding, ShaderResourceType::UNIFORM_BUFFER_DYNAMIC});
        return device->CreateShader(shaderDesc);
    }

} // namespace

TEST_F(AuroraVulkanTest, BatchDynamicUboStableBinding)
{
    auto *device = GetDevice();

    uint32_t set = 0, binding = 0, blockSize = 0;
    auto     batchShader = CounterPtr<Shader>(MakeDynamicBatchShader(device, set, binding, blockSize));
    ASSERT_NE(batchShader.Get(), nullptr);
    ASSERT_GT(blockSize, 0u);

    // batch RG: descriptor set layout derives a UNIFORM_BUFFER_DYNAMIC binding
    ResourceGroup::Descriptor rgDesc{};
    rgDesc.shader = batchShader.Get();
    rgDesc.set    = set;
    auto group    = CounterPtr<ResourceGroup>(device->CreateResourceGroup(rgDesc));
    ASSERT_NE(group.Get(), nullptr);

    // pack buffer (host-visible; one frame worth of packed blocks)
    Buffer::Descriptor bufDesc{};
    bufDesc.size   = blockSize * 4;
    bufDesc.usage  = BufferUsageFlagBit::UNIFORM;
    bufDesc.memory = MemoryType::CPU_TO_GPU;
    auto buffer    = CounterPtr<Buffer>(device->CreateBuffer(bufDesc));
    ASSERT_NE(buffer.Get(), nullptr);

    // stable binding: offset=0, range=blockSize, written once per frame
    auto encoder = group->CreateEncoder();
    ASSERT_TRUE(encoder->WriteBuffer(binding, buffer.Get(), 0, blockSize));
    ASSERT_TRUE(encoder->End());

    auto encoder2 = group->CreateEncoder();
    encoder2->WriteBuffer(binding, buffer.Get(), 0, blockSize);
    encoder2->End(); // a second frame bind with the same explicit range also succeeds
}

TEST_F(AuroraVulkanTest, BatchDynamicUboRangeZeroRejected)
{
    auto *device = GetDevice();

    uint32_t set = 0, binding = 0, blockSize = 0;
    auto     batchShader = CounterPtr<Shader>(MakeDynamicBatchShader(device, set, binding, blockSize));
    ASSERT_NE(batchShader.Get(), nullptr);

    ResourceGroup::Descriptor rgDesc{};
    rgDesc.shader = batchShader.Get();
    rgDesc.set    = set;
    auto group    = CounterPtr<ResourceGroup>(device->CreateResourceGroup(rgDesc));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bufDesc{};
    bufDesc.size   = 1024;
    bufDesc.usage  = BufferUsageFlagBit::UNIFORM;
    bufDesc.memory = MemoryType::CPU_TO_GPU;
    auto buffer    = CounterPtr<Buffer>(device->CreateBuffer(bufDesc));
    ASSERT_NE(buffer.Get(), nullptr);

    auto enc = group->CreateEncoder();
    EXPECT_FALSE(enc->WriteBuffer(binding, buffer.Get(), 0, 0)); // dynamic UBO requires an explicit range
    EXPECT_FALSE(enc->End());
}

TEST_F(AuroraVulkanTest, ReflectionValidationMatch)
{
    RgBlockDesc desc{};
    desc.set       = 0;
    desc.binding   = 0;
    desc.blockName = Name("Global");
    desc.kind      = RgBlockKind::CBUFFER;
    desc.fields    = {{RgFieldType::MAT4, Name("ViewProj")}};

    ShaderReflection refl{};
    ShaderResource   res{};
    res.name    = "Global";
    res.set     = 0;
    res.binding = 0;
    res.type    = ShaderResourceType::UNIFORM_BUFFER;
    refl.resources.push_back(res);

    EXPECT_TRUE(ValidateBlockAgainstReflection(desc, refl).empty());
}

TEST_F(AuroraVulkanTest, ReflectionValidationMismatch)
{
    RgBlockDesc desc{};
    desc.set       = 0;
    desc.binding   = 0;
    desc.blockName = Name("Global");
    desc.kind      = RgBlockKind::CBUFFER;

    ShaderReflection refl{};
    ShaderResource   res{};
    res.name    = "Global";
    res.set     = 0;
    res.binding = 0;
    res.type    = ShaderResourceType::SAMPLED_IMAGE; // wrong type
    refl.resources.push_back(res);

    EXPECT_FALSE(ValidateBlockAgainstReflection(desc, refl).empty());

    // not present at all
    ShaderReflection empty{};
    EXPECT_FALSE(ValidateBlockAgainstReflection(desc, empty).empty());
}

TEST_F(AuroraVulkanTest, SharedBlockDescMatchesReflection)
{
    const char *shader = R"(
#include <aurora/pipeline/GlobalBlock.slangh>
[shader("fragment")]
float4 mainFS() : SV_Target { return gGlobal.frameTime; }
)";

    ShaderFileSystem files;
    files.AddSearchPath(AURORA_SHADER_ASSET_DIR);
    ShaderCompilerSlang compiler;
    ShaderCompileDesc   d{};
    d.source     = shader;
    d.entry      = "mainFS";
    d.stage      = ShaderStageFlagBit::FS;
    d.target     = ShaderTarget::SPIRV;
    d.fileSystem = &files;

    ShaderCompileResult r{};
    ASSERT_TRUE(compiler.Compile(d, r)) << r.errorInfo;

    // the shared RgBlockDesc (from GlobalBlock.h) must match the
    // shader reflection of the same block
    EXPECT_TRUE(ValidateBlockAgainstReflection(shared::GetGlobalParamsBlockDesc(), r.reflection).empty());
}
