//
// RgBlockDesc / ShaderBlockGen tests: layout generation, HLSL text, consistency.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/rg/RgBlockDesc.h>
#include <aurora/pipeline/rg/ShaderBlockGen.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, RgBlockDescToLayout)
{
    RgBlockDesc desc{};
    desc.set       = 0;
    desc.binding   = 0;
    desc.blockName = Name("Global");
    desc.kind      = RgBlockKind::CBUFFER;
    desc.fields    = {
        {RgFieldType::MAT4, Name("ViewProj")},
        {RgFieldType::FLOAT4, Name("CameraPos")},
    };

    const auto layout = ToLayoutDescriptor(desc);
    ASSERT_EQ(layout.bindings.size(), 1u);
    EXPECT_EQ(layout.bindings[0].binding, 0u);
    EXPECT_EQ(layout.bindings[0].type, DescriptorType::UNIFORM_BUFFER);
    EXPECT_EQ(layout.bindings[0].count, 1u);
}

TEST_F(AuroraVulkanTest, RgBlockDescDynamicKind)
{
    RgBlockDesc desc{};
    desc.kind = RgBlockKind::CBUFFER_DYNAMIC;

    const auto layout = ToLayoutDescriptor(desc);
    ASSERT_EQ(layout.bindings.size(), 1u);
    EXPECT_EQ(layout.bindings[0].type, DescriptorType::UNIFORM_BUFFER_DYNAMIC);
}

TEST_F(AuroraVulkanTest, RgBlockDescFieldOffsets)
{
    RgBlockDesc desc{};
    desc.fields = {
        {RgFieldType::MAT4, Name("World")},       // 64B at offset 0
        {RgFieldType::FLOAT, Name("Intensity")},  // 4B at offset 64
        {RgFieldType::FLOAT3, Name("Color")},     // vec3: 16B aligned -> offset 80
    };

    uint32_t total = 0;
    const auto layouts = ComputeFieldOffsets(desc, total);

    ASSERT_EQ(layouts.size(), 3u);
    EXPECT_EQ(layouts[0].offset, 0u);
    EXPECT_EQ(layouts[0].size, 64u);
    EXPECT_EQ(layouts[1].offset, 64u);
    EXPECT_EQ(layouts[1].size, 4u);
    EXPECT_EQ(layouts[2].offset, 80u); // 16B aligned, not 68
    EXPECT_EQ(layouts[2].size, 16u);
    EXPECT_EQ(total, 96u);
}

TEST_F(AuroraVulkanTest, ShaderBlockGenCbufferText)
{
    RgBlockDesc desc{};
    desc.set       = 0;
    desc.binding   = 0;
    desc.blockName = Name("Global");
    desc.fields    = {
        {RgFieldType::MAT4, Name("ViewProj")},
        {RgFieldType::FLOAT4, Name("CameraPos")},
    };

    const std::string text = ShaderBlockGen::GenerateHlsl(desc);
    EXPECT_NE(text.find("[[vk::binding(0, 0)]]"), std::string::npos);
    EXPECT_NE(text.find("cbuffer Global : register(b0, space0)"), std::string::npos);
    EXPECT_NE(text.find("float4x4 ViewProj;"), std::string::npos);
    EXPECT_NE(text.find("float4 CameraPos;"), std::string::npos);
}

TEST_F(AuroraVulkanTest, ShaderBlockGenTextureResource)
{
    RgBlockDesc desc{};
    desc.set       = 1;
    desc.binding   = 2;
    desc.blockName = Name("ShadowMap");
    desc.kind      = RgBlockKind::RESOURCE;
    desc.fields    = {{RgFieldType::TEXTURE2D, Name("ShadowMap")}};

    const std::string text = ShaderBlockGen::GenerateHlsl(desc);
    EXPECT_NE(text.find("[[vk::binding(2, 1)]]"), std::string::npos);
    EXPECT_NE(text.find("Texture2D ShadowMap : register(t2, space1)"), std::string::npos);
}

TEST_F(AuroraVulkanTest, ShaderBlockGenConsistency)
{
    RgBlockDesc desc{};
    desc.set       = 1;
    desc.binding   = 0;
    desc.blockName = Name("PassData");
    desc.fields    = {{RgFieldType::FLOAT4, Name("Params")}};

    // same desc must produce matching binding/set on both sides
    const auto layout = ToLayoutDescriptor(desc);
    const std::string text = ShaderBlockGen::GenerateHlsl(desc);

    EXPECT_EQ(layout.bindings[0].binding, desc.binding);
    EXPECT_NE(text.find("register(b0, space1)"), std::string::npos);
}

TEST_F(AuroraVulkanTest, ShaderBlockGenHashStable)
{
    RgBlockDesc desc{};
    desc.blockName = Name("Global");
    desc.fields    = {{RgFieldType::MAT4, Name("ViewProj")}};

    const uint32_t h1 = ShaderBlockGen::ContentHash(desc);
    const uint32_t h2 = ShaderBlockGen::ContentHash(desc);
    EXPECT_EQ(h1, h2); // deterministic across calls

    desc.fields.push_back({RgFieldType::FLOAT4, Name("CameraPos")});
    EXPECT_NE(ShaderBlockGen::ContentHash(desc), h1); // change invalidates
}
