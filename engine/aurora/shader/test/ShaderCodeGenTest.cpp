//
// ShaderCodeGen tests: reflected block -> C++ mirror header + virtual include.
//

#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/gen/ShaderCodeGen.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace sky;
using namespace sky::aurora;

namespace {

    const char *kBlockHeader = R"(
struct GlobalParams {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
};
[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal;
)";

    const char *kIncludeShader = R"(
#include "global/GlobalBlock.slang"

[shader("fragment")]
float4 mainFS() : SV_Target
{
    return gGlobal.cameraPos;
}
)";

    const ShaderBlockLayout *FindGlobalBlock(const ShaderReflection &reflection)
    {
        for (const auto &block : reflection.blocks) {
            if (block.structName == "GlobalParams") {
                return &block;
            }
        }
        return nullptr;
    }

    // reflect the header-only GlobalBlock (no entry point)
    const ShaderBlockLayout *ReflectGlobalBlock(ShaderCompilerSlang &compiler,
                                                ShaderReflection &reflection)
    {
        std::string error;
        if (!compiler.ReflectBlocks(kBlockHeader, ShaderTarget::SPIRV, reflection, error)) {
            return nullptr;
        }
        return FindGlobalBlock(reflection);
    }

    bool CompileWithFileSystem(ShaderFileSystem &fs, ShaderCompileResult &result)
    {
        ShaderCompilerSlang compiler;
        ShaderCompileDesc desc{};
        desc.source     = kIncludeShader;
        desc.entry      = "mainFS";
        desc.stage      = ShaderStageFlagBit::FS;
        desc.target     = ShaderTarget::SPIRV;
        desc.fileSystem = &fs;
        return compiler.Compile(desc, result);
    }

} // namespace

TEST(ShaderCodeGenTest, VirtualInclude)
{
    ShaderFileSystem fs;
    fs.AddVirtualFile("global/GlobalBlock.slang", kBlockHeader);

    ShaderCompileResult result{};
    ASSERT_TRUE(CompileWithFileSystem(fs, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    const ShaderBlockLayout *block = FindGlobalBlock(result.reflection);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->members.size(), 4u);
}

TEST(ShaderCodeGenTest, SearchPathInclude)
{
    const auto temp = std::filesystem::temp_directory_path() / "aurora_shader_fs_test";
    std::filesystem::create_directories(temp / "global");
    {
        std::ofstream out(temp / "global" / "GlobalBlock.slang", std::ios::binary);
        out << kBlockHeader;
    }

    ShaderFileSystem fs;
    fs.AddSearchPath(temp.string());

    ShaderCompileResult result{};
    ASSERT_TRUE(CompileWithFileSystem(fs, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    std::filesystem::remove_all(temp);
}

TEST(ShaderCodeGenTest, ReflectTypeInfo)
{
    ShaderCompilerSlang compiler;
    ShaderReflection reflection;
    const ShaderBlockLayout *block = ReflectGlobalBlock(compiler, reflection);
    ASSERT_NE(block, nullptr);
    ASSERT_EQ(block->members.size(), 4u);

    EXPECT_EQ(block->set, 0u);
    EXPECT_EQ(block->binding, 0u);

    EXPECT_EQ(block->members[0].name, "view");
    EXPECT_EQ(block->members[0].kind, ShaderTypeKind::MATRIX);
    EXPECT_EQ(block->members[0].scalarType, ShaderScalarType::FLOAT);
    EXPECT_EQ(block->members[0].rows, 4u);
    EXPECT_EQ(block->members[0].cols, 4u);

    EXPECT_EQ(block->members[3].name, "cameraPos");
    EXPECT_EQ(block->members[3].kind, ShaderTypeKind::VECTOR);
    EXPECT_EQ(block->members[3].cols, 4u);
    EXPECT_EQ(block->members[3].offset, 192u);
}

TEST(ShaderCodeGenTest, GenerateCppHeader)
{
    ShaderCompilerSlang compiler;
    ShaderReflection reflection;
    const ShaderBlockLayout *block = ReflectGlobalBlock(compiler, reflection);
    ASSERT_NE(block, nullptr);

    std::string header;
    std::string error;
    ASSERT_TRUE(ShaderCodeGen::GenerateCppHeader(*block, header, error)) << error;

    EXPECT_NE(header.find("struct GlobalParams"), std::string::npos);
    EXPECT_NE(header.find("alignas(16) Matrix4 view;"), std::string::npos);
    EXPECT_NE(header.find("alignas(16) Vector4 cameraPos;"), std::string::npos);
    EXPECT_NE(header.find("static_assert(sizeof(GlobalParams) == 208"), std::string::npos);
    EXPECT_NE(header.find("static_assert(offsetof(GlobalParams, cameraPos) == 192"),
              std::string::npos);
    EXPECT_NE(header.find("GetGlobalParamsBlockDesc()"), std::string::npos);
}

TEST(ShaderCodeGenTest, BuildBlockDesc)
{
    ShaderCompilerSlang compiler;
    ShaderReflection reflection;
    const ShaderBlockLayout *block = ReflectGlobalBlock(compiler, reflection);
    ASSERT_NE(block, nullptr);

    RgBlockDesc blockDesc{};
    std::string error;
    ASSERT_TRUE(ShaderCodeGen::BuildBlockDesc(*block, blockDesc, error)) << error;

    EXPECT_EQ(blockDesc.blockName, Name("GlobalParams"));
    EXPECT_EQ(blockDesc.kind, RgBlockKind::CBUFFER);
    ASSERT_EQ(blockDesc.fields.size(), 4u);
    EXPECT_EQ(blockDesc.fields[0].type, RgFieldType::MAT4);
    EXPECT_EQ(blockDesc.fields[3].type, RgFieldType::FLOAT4);
}
