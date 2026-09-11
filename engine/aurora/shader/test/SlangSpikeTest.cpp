//
// Slang spike: compile a mini shader with ParameterBlocks to SPIRV + MSL.
//

#include <aurora/shader/ShaderCompilerSlang.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>

using namespace sky;
using namespace sky::aurora;

namespace {

    const char *kSpikeShader = R"(
struct GlobalParams { float4x4 viewProj; float4 cameraPos; };
struct PassParams   { float4 params; };

ParameterBlock<GlobalParams> gGlobal;
ParameterBlock<PassParams>   gPass;
Texture2D<float4>            gTex;
SamplerState                 gSmp;

struct FSOutput { float4 color : SV_Target; };

[shader("fragment")]
FSOutput mainFS(float2 uv : TEXCOORD0)
{
    FSOutput o;
    o.color = gTex.Sample(gSmp, uv) * gPass.params + gGlobal.cameraPos;
    return o;
}
)";

    std::string WordsToText(const std::vector<uint32_t> &data)
    {
        std::string out(data.size() * sizeof(uint32_t), '\0');
        std::memcpy(out.data(), data.data(), out.size());
        out.erase(out.find_last_not_of('\0') + 1);
        return out;
    }

} // namespace

TEST(SlangSpikeTest, CompileSpirvWithParameterBlocks)
{
    ShaderCompilerSlang compiler;

    ShaderCompileDesc desc{};
    desc.source = kSpikeShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.target = ShaderTarget::SPIRV;

    ShaderCompileResult result{};
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    // slang natural layout: ordinary resources -> set 0; each ParameterBlock gets
    // its own descriptor set in declaration order (global=1, pass=2)
    bool foundGlobal = false, foundPass = false, foundTex = false;
    for (const auto &res : result.reflection.resources) {
        if (res.set == 1 && res.binding == 0 && res.type == ShaderResourceType::UNIFORM_BUFFER) { foundGlobal = true; }
        if (res.set == 2 && res.binding == 0 && res.type == ShaderResourceType::UNIFORM_BUFFER) { foundPass = true; }
        if (res.set == 0 && res.binding == 0 && res.type == ShaderResourceType::SAMPLED_IMAGE)  { foundTex = true; }
    }
    EXPECT_TRUE(foundGlobal);
    EXPECT_TRUE(foundPass);
    EXPECT_TRUE(foundTex);

    // UBO member reflection: blocks carry field name/offset/size
    const ShaderBlockLayout *globalBlock = nullptr;
    for (const auto &block : result.reflection.blocks) {
        if (block.name == "gGlobal") {
            globalBlock = &block;
        }
    }
    ASSERT_NE(globalBlock, nullptr);
    ASSERT_EQ(globalBlock->members.size(), 2u);
    EXPECT_STREQ(globalBlock->members[0].name.c_str(), "viewProj");
    EXPECT_EQ(globalBlock->members[0].offset, 0u);
    EXPECT_EQ(globalBlock->members[0].size, 64u);
    EXPECT_STREQ(globalBlock->members[1].name.c_str(), "cameraPos");
    EXPECT_EQ(globalBlock->members[1].offset, 64u); // after mat4
    EXPECT_EQ(globalBlock->members[1].size, 16u);
}

// DX12 backend: DXIL via slang's runtime DXC loader (repo's dxcompiler package)
TEST(SlangSpikeTest, CompileDxilForDx12)
{
    ShaderCompilerSlang compiler;

    ShaderCompileDesc desc{};
    desc.source = kSpikeShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.target = ShaderTarget::DXIL;

    ShaderCompileResult result{};
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    // DXIL binary starts with the DXBC magic "DXBC"
    const auto &words = result.data;
    ASSERT_GE(words.size(), 1u);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(words.data());
    EXPECT_EQ(bytes[0], 'D');
    EXPECT_EQ(bytes[1], 'X');
    EXPECT_EQ(bytes[2], 'B');
    EXPECT_EQ(bytes[3], 'C');

    // slang reflection on DXIL path: ParameterBlocks still visible
    EXPECT_FALSE(result.reflection.resources.empty());
}

TEST(SlangSpikeTest, CompileMslDirectNoSpirvCross)
{
    ShaderCompilerSlang compiler;

    ShaderCompileDesc desc{};
    desc.source = kSpikeShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.target = ShaderTarget::MSL;

    ShaderCompileResult result{};
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    const std::string msl = WordsToText(result.data);
    // ParameterBlock -> argument buffer style: constant pointer at [[buffer(N)]]
    EXPECT_NE(msl.find("[[buffer("), std::string::npos);
    EXPECT_NE(msl.find("constant"), std::string::npos);
    EXPECT_NE(msl.find("[[fragment]]"), std::string::npos);
    EXPECT_NE(msl.find("metal_stdlib"), std::string::npos);
    // #line directives are disabled (SLANG_LINE_DIRECTIVE_MODE_NONE)
    EXPECT_EQ(msl.find("#line"), std::string::npos);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
