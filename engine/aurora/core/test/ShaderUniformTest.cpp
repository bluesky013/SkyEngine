#include <aurora/shader/ShaderUniform.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector4.h>
#include <cstring>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

TEST(ShaderUniform, CombinedAssemblerUsesTargetOffsetsAndRejectsIncompleteOrConflictingFields)
{
    ShaderBlockLayout block{"combined",
                            "Combined",
                            2,
                            5,
                            96,
                            {{"world", 0, 64, ShaderScalarType::FLOAT, ShaderTypeKind::MATRIX, 4, 4},
                             {"tint", 80, 16, ShaderScalarType::FLOAT, ShaderTypeKind::VECTOR, 1, 4}}};
    block.alignment                       = 16;
    block.members[0].alignment            = 16;
    block.members[0].matrixStride         = 16;
    block.members[1].alignment            = 16;
    const Matrix4                   world = Matrix4::Identity();
    const Vector4                   tint{.2f, .4f, .6f, 1};
    std::vector<ShaderUniformValue> values{{"tint", RgFieldType::FLOAT4, std::as_bytes(std::span{&tint, 1})},
                                           {"world", RgFieldType::MAT4, std::as_bytes(std::span{&world, 1})}};
    const auto                      packed = PackShaderUniform(block, values);
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(packed));
    const auto &bytes = std::get<std::vector<std::byte>>(packed);
    ASSERT_EQ(bytes.size(), 96u);
    EXPECT_EQ(std::memcmp(bytes.data(), &world, sizeof(world)), 0);
    EXPECT_EQ(std::memcmp(bytes.data() + 80, &tint, sizeof(tint)), 0);
    for (size_t i = 64; i < 80; ++i)
        EXPECT_EQ(bytes[i], std::byte{0});
    auto invalid = values;
    invalid.pop_back();
    EXPECT_TRUE(std::holds_alternative<std::string>(PackShaderUniform(block, invalid)));
    invalid    = values;
    invalid[1] = invalid[0];
    EXPECT_TRUE(std::holds_alternative<std::string>(PackShaderUniform(block, invalid)));
    invalid         = values;
    invalid[0].type = RgFieldType::UINT;
    EXPECT_TRUE(std::holds_alternative<std::string>(PackShaderUniform(block, invalid)));
    block.members[1].offset = 48;
    EXPECT_TRUE(std::holds_alternative<std::string>(PackShaderUniform(block, values)));
}
