//
// Created on 2026/10/23.
//

#include <aurora/rhi/ShaderParameters.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

using namespace sky;
using namespace sky::aurora;

namespace {
    ShaderBlockMember Float4(const std::string &name, uint32_t offset)
    {
        return {.name       = name,
                .offset     = offset,
                .size       = 16,
                .scalarType = ShaderScalarType::FLOAT,
                .kind       = ShaderTypeKind::VECTOR,
                .cols       = 4,
                .alignment  = 16};
    }
    float Read(const std::vector<std::byte> &bytes, uint32_t offset)
    {
        float value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
} // namespace

TEST(UniformUploadPlan, DifferentOffsetsAndMatrixMajorIgnoreSourcePadding)
{
    ShaderBlockMember scalar{.name = "exposure", .size = 4, .scalarType = ShaderScalarType::FLOAT, .kind = ShaderTypeKind::SCALAR, .alignment = 4};
    auto              matrix = Float4("transform", 32);
    matrix.kind              = ShaderTypeKind::MATRIX;
    matrix.rows              = 4;
    matrix.size              = 64;
    matrix.matrixStride      = 16;
    ShaderCpuSchema cpu{96, 16, {scalar, Float4("color", 16), matrix}};
    auto            targetScalar = scalar;
    targetScalar.offset          = 32;
    auto targetMatrix            = matrix;
    targetMatrix.offset          = 64;
    targetMatrix.matrixLayout    = ShaderMatrixLayout::ROW_MAJOR;
    ShaderBlockLayout      target{.name = "params", .size = 144, .members = {Float4("color", 0), targetScalar, targetMatrix}, .alignment = 16};
    std::vector<std::byte> source(cpu.size, std::byte{0xcd});
    const float            exposure    = 2.5f;
    const float            color[]     = {4, 3, 2, 1};
    const float            transform[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    std::memcpy(source.data(), &exposure, 4);
    std::memcpy(source.data() + 16, color, 16);
    std::memcpy(source.data() + 32, transform, 64);
    UniformUploadPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildUniformUploadPlan(cpu, target, 37, plan, error)) << error;
    EXPECT_FALSE(plan.wholeCopy);
    std::vector<std::byte> bytes;
    ASSERT_TRUE(EncodeShaderUniform(plan, source, bytes, error)) << error;
    EXPECT_FLOAT_EQ(Read(bytes, 32), exposure);
    for (uint32_t i = 0; i < 4; ++i)
        EXPECT_FLOAT_EQ(Read(bytes, 4 * i), color[i]);
    for (uint32_t row = 0; row < 4; ++row)
        for (uint32_t column = 0; column < 4; ++column)
            EXPECT_FLOAT_EQ(Read(bytes, 64 + row * 16 + column * 4), transform[column * 4 + row]);
    for (uint32_t i = 16; i < 32; ++i)
        EXPECT_EQ(bytes[i], std::byte{});
    for (uint32_t i = 36; i < 64; ++i)
        EXPECT_EQ(bytes[i], std::byte{});
    for (uint32_t i = 128; i < 144; ++i)
        EXPECT_EQ(bytes[i], std::byte{});
}

TEST(UniformUploadPlan, ArrayStrideAndTypedAssemblerUseTheSameEncoding)
{
    auto array        = Float4("weights", 0);
    array.kind        = ShaderTypeKind::ARRAY;
    array.arrayCount  = 2;
    array.arrayStride = 16;
    array.size        = 32;
    ShaderCpuSchema cpu{32, 16, {array}};
    auto            native = array;
    native.offset          = 16;
    native.arrayStride     = 32;
    native.size            = 64;
    ShaderBlockLayout block{.name = "params", .size = 80, .members = {native}, .alignment = 16};
    const Vector4     values[] = {{1, 2, 3, 4}, {5, 6, 7, 8}};
    UniformUploadPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildUniformUploadPlan(cpu, block, 5, plan, error)) << error;
    std::vector<std::byte> typed, encoded;
    ASSERT_TRUE(EncodeShaderUniform(plan, std::as_bytes(std::span(values)), encoded, error));
    const std::pair<std::string, ShaderDataValue> named[] = {{"weights", MakeShaderData(std::span<const Vector4>(values))}};
    ASSERT_TRUE(PackShaderData(block, named, typed, error)) << error;
    EXPECT_EQ(typed, encoded);
    EXPECT_FLOAT_EQ(Read(typed, 16), 1);
    EXPECT_FLOAT_EQ(Read(typed, 48), 5);
    for (uint32_t i = 32; i < 48; ++i)
        EXPECT_EQ(typed[i], std::byte{});
}

TEST(UniformUploadPlan, WholeCopyRequiresCoverageOfEveryByte)
{
    ShaderCpuSchema   cpu{32, 16, {Float4("first", 0), Float4("second", 16)}};
    ShaderBlockLayout block{.name = "params", .size = 32, .members = cpu.fields, .alignment = 16};
    UniformUploadPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildUniformUploadPlan(cpu, block, 1, plan, error));
    EXPECT_TRUE(plan.wholeCopy);
    std::vector<std::byte> source(32, std::byte{0xab}), target;
    ASSERT_TRUE(EncodeShaderUniform(plan, source, target, error));
    EXPECT_EQ(source, target);
    auto scalar   = cpu.fields[0];
    scalar.kind   = ShaderTypeKind::SCALAR;
    scalar.cols   = 1;
    scalar.size   = 4;
    cpu.fields[0] = scalar;
    block.members = cpu.fields;
    ASSERT_TRUE(BuildUniformUploadPlan(cpu, block, 1, plan, error));
    EXPECT_FALSE(plan.wholeCopy);
    ASSERT_TRUE(EncodeShaderUniform(plan, source, target, error));
    for (uint32_t i = 4; i < 16; ++i)
        EXPECT_EQ(target[i], std::byte{});
}

TEST(UniformUploadPlan, InvalidSchemasAndFailedEncodingPreserveThePreviousResult)
{
    ShaderCpuSchema   cpu{32, 16, {Float4("first", 0), Float4("second", 16)}};
    ShaderBlockLayout block{.name = "params", .size = 32, .members = cpu.fields, .alignment = 16};
    UniformUploadPlan plan;
    std::string       error;
    ASSERT_TRUE(BuildUniformUploadPlan(cpu, block, 9, plan, error));
    const auto oldKey             = plan.key;
    auto       invalid            = block;
    invalid.members[0].scalarType = ShaderScalarType::UINT;
    EXPECT_FALSE(BuildUniformUploadPlan(cpu, invalid, 9, plan, error));
    EXPECT_EQ(plan.key, oldKey);
    invalid                   = block;
    invalid.members[1].offset = 8;
    EXPECT_FALSE(BuildUniformUploadPlan(cpu, invalid, 9, plan, error));
    invalid                   = block;
    invalid.members[1].offset = UINT32_MAX;
    EXPECT_FALSE(BuildUniformUploadPlan(cpu, invalid, 9, plan, error));
    invalid                 = block;
    invalid.members[1].name = "first";
    EXPECT_FALSE(BuildUniformUploadPlan(cpu, invalid, 9, plan, error));
    invalid                 = block;
    invalid.members[1].cols = 3;
    EXPECT_FALSE(BuildUniformUploadPlan(cpu, invalid, 9, plan, error));
    std::vector<std::byte> previous{std::byte{0x42}}, shortSource(31);
    EXPECT_FALSE(EncodeShaderUniform(plan, shortSource, previous, error));
    EXPECT_EQ(previous, std::vector<std::byte>{std::byte{0x42}});
}
