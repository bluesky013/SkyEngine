//
// Created by blues on 2025/4/21.
//

#include <gtest/gtest.h>
#include <shader/node/ShaderDataType.h>
#include <shader/node/ShaderNode.h>
#include <shader/node/ResourceGroupDecl.h>
#include <shader/node/BufferLayoutCalculator.h>
#include <shader/node/ResourceDeclGenerator.h>
#include <shader/node/RHILayoutGenerator.h>

using namespace sky;
using namespace sky::sl;

// -- Compile-time layout verification (static_assert) ----------

// Scalar
static_assert(CalculateTypeLayout({ShaderBaseType::FLOAT, ShaderDataType::SCALAR}, LayoutStandard::STD140).size == 4);
static_assert(CalculateTypeLayout({ShaderBaseType::FLOAT, ShaderDataType::SCALAR}, LayoutStandard::STD140).alignment == 4);

// Vector4
static_assert(CalculateTypeLayout({ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}, LayoutStandard::STD140).size == 16);
static_assert(CalculateTypeLayout({ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}, LayoutStandard::STD140).alignment == 16);

// Matrix4x4
static_assert(CalculateTypeLayout({ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}, LayoutStandard::STD140).size == 64);

// passInfo members -- compile-time offset verification
static constexpr MemberDecl kPassInfoMembers[] = {
    {"LightMatrix",       {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
    {"MainLightColor",    {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    {"MainLightDirection",{ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    {"Viewport",          {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
};
static_assert(CalculateMembersSize(kPassInfoMembers, {}, LayoutStandard::STD140) == 112);
static_assert(CalculateMemberOffset(kPassInfoMembers, {}, LayoutStandard::STD140, 0) == 0);   // LightMatrix
static_assert(CalculateMemberOffset(kPassInfoMembers, {}, LayoutStandard::STD140, 1) == 64);  // MainLightColor
static_assert(CalculateMemberOffset(kPassInfoMembers, {}, LayoutStandard::STD140, 2) == 80);  // MainLightDirection
static_assert(CalculateMemberOffset(kPassInfoMembers, {}, LayoutStandard::STD140, 3) == 96);  // Viewport

// ViewInfo -- UBO struct with 4 matrices
static constexpr MemberDecl kViewInfoMembers[] = {
    {"World",        {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
    {"View",         {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
    {"ViewProj",     {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
    {"LastViewProj", {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
};
static constexpr StructDecl kViewInfo = {"ViewInfo", kViewInfoMembers, StructUsage::UBO};
static_assert(CalculateStructSize(kViewInfo) == 256);

// Meshlet -- SSBO struct (std430)
static constexpr MemberDecl kMeshletMembers[] = {
    {"vertexOffset",   {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
    {"triangleOffset", {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
    {"vertexCount",    {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
    {"triangleCount",  {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
    {"center",         {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    {"coneApex",       {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    {"coneAxis",       {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
};
static constexpr StructDecl kMeshlet = {"Meshlet", kMeshletMembers, StructUsage::SSBO};
static_assert(CalculateStructSize(kMeshlet) == 64);
static_assert(CalculateMemberOffset(kMeshletMembers, {}, LayoutStandard::STD430, 4) == 16);  // center

// Nested struct: BVHNode contains AABB (compile-time resolution)
static constexpr MemberDecl kAABBMembers[] = {
    {"minBound", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    {"maxBound", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
};
static constexpr MemberDecl kBVHNodeMembers[] = {
    {"bounds",      {ShaderBaseType::NONE, ShaderDataType::STRUCT}, 0, "AABB"},
    {"leftChild",   {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
    {"rightChild",  {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
    {"primitiveId", {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
    {"flags",       {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
};
static constexpr StructDecl kBVHStructs[] = {
    {"AABB",    kAABBMembers,    StructUsage::PLAIN},
    {"BVHNode", kBVHNodeMembers, StructUsage::SSBO},
};
static_assert(CalculateStructSize(kBVHStructs[0], kBVHStructs) == 32);  // AABB
static_assert(CalculateStructSize(kBVHStructs[1], kBVHStructs) == 48);  // BVHNode

// --- Helper: build DefaultPass ResourceGroupDecl ---

static ResourceGroupDecl BuildDefaultPassGroup()
{
    // Local struct: ViewInfo (UBO -- referenced by cbuffer viewInfo)
    static constexpr MemberDecl viewInfoMembers[] = {
        {"World",        {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
        {"View",         {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
        {"ViewProj",     {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
        {"LastViewProj", {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
    };
    static constexpr StructDecl localStructs[] = {
        {"ViewInfo", viewInfoMembers, StructUsage::UBO},
    };

    // passInfo cbuffer (binding 0)
    static constexpr MemberDecl passInfoMembers[] = {
        {"LightMatrix",       {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
        {"MainLightColor",    {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"MainLightDirection",{ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"Viewport",          {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };

    // viewInfo cbuffer (binding 1)
    static constexpr MemberDecl viewInfoCBMembers[] = {
        {"View", {ShaderBaseType::NONE, ShaderDataType::STRUCT, 0, 0}, 0, "ViewInfo"},
    };

    static constexpr ResourceDecl resources[] = {
        {.type = ResourceType::CONSTANT_BUFFER, .name = "passInfo",  .members = passInfoMembers},
        {.type = ResourceType::CONSTANT_BUFFER, .name = "viewInfo",  .members = viewInfoCBMembers},
        {.type = ResourceType::TEXTURE,  .name = "ShadowMap"},
        {.type = ResourceType::SAMPLER,  .name = "ShadowMapSampler"},
        {.type = ResourceType::TEXTURE,  .name = "BRDFLut"},
        {.type = ResourceType::SAMPLER,  .name = "BRDFLutSampler"},
        {.type = ResourceType::TEXTURE,  .name = "IrradianceMap",       .texType = TextureType::TEXTURE_CUBE},
        {.type = ResourceType::SAMPLER,  .name = "IrradianceSampler"},
        {.type = ResourceType::TEXTURE,  .name = "PrefilteredMap",      .texType = TextureType::TEXTURE_CUBE},
        {.type = ResourceType::SAMPLER,  .name = "PrefilteredMapSampler"},
        {.type = ResourceType::TEXTURE,  .name = "HizBuffer"},
        {.type = ResourceType::SAMPLER,  .name = "HizBufferSampler"},
    };

    return ResourceGroupDecl{
        "DefaultPass",
        0,
        rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS,
        localStructs,
        resources,
    };
}

// --- Helper: build DefaultLocal ResourceGroupDecl with MESH_SHADER conditional ---

static ResourceGroupDecl BuildDefaultLocalGroup()
{
    // Local structs: Meshlet and ExtVertex (SSBO -- referenced by StructuredBuffers)
    static constexpr MemberDecl meshletMembers[] = {
        {"vertexOffset",   {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
        {"triangleOffset", {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
        {"vertexCount",    {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
        {"triangleCount",  {ShaderBaseType::UINT,  ShaderDataType::SCALAR}},
        {"center",         {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"coneApex",       {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"coneAxis",       {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };
    static constexpr MemberDecl extVertexMembers[] = {
        {"uv",      {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"normal",  {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"tangent", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"color",   {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };
    static constexpr StructDecl localStructs[] = {
        {"Meshlet",   meshletMembers,   StructUsage::SSBO},
        {"ExtVertex", extVertexMembers, StructUsage::SSBO},
    };

    // Local cbuffer (binding 0)
    static constexpr MemberDecl localCBMembers[] = {
        {"World",        {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
        {"InverseTrans", {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
    };

    static constexpr ResourceDecl resources[] = {
        {.type = ResourceType::CONSTANT_BUFFER, .name = "Local", .members = localCBMembers, .dynamic = true},
    };

    // MESH_SHADER conditional block
    static constexpr MemberDecl meshletInfoMembers[] = {
        {"FirstMeshlet", {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {"MeshletCount", {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
    };

    static constexpr ResourceDecl meshResources[] = {
        {.type = ResourceType::CONSTANT_BUFFER,   .name = "MeshletInfo",       .members = meshletInfoMembers, .dynamic = true},
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "PositionBuf",       .elementType = {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "ExtBuf",            .elementStructRef = "ExtVertex"},
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "VertexIndices",     .elementType = {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "MeshletTriangles",  .elementType = {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "Meshlets",          .elementStructRef = "Meshlet"},
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "InstanceBuffer",    .elementType = {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };

    static constexpr ConditionalBlock conditionals[] = {
        {"MESH_SHADER", meshResources},
    };

    return ResourceGroupDecl{
        "DefaultLocal",
        2,
        rhi::ShaderStageFlagBit::VS,
        localStructs,
        resources,
        conditionals,
    };
}

// ---------------------------------------------
//  Layout Calculator Tests
// ---------------------------------------------

TEST(ShaderLayoutTest, ScalarTypeLayout)
{
    auto info = BufferLayoutCalculator::CalculateType(
        {ShaderBaseType::FLOAT, ShaderDataType::SCALAR}, LayoutStandard::STD140);
    EXPECT_EQ(info.size, 4u);
    EXPECT_EQ(info.alignment, 4u);
}

TEST(ShaderLayoutTest, Vec4Layout)
{
    auto info = BufferLayoutCalculator::CalculateType(
        {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}, LayoutStandard::STD140);
    EXPECT_EQ(info.size, 16u);
    EXPECT_EQ(info.alignment, 16u);
}

TEST(ShaderLayoutTest, Vec3Layout)
{
    auto info = BufferLayoutCalculator::CalculateType(
        {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 3, 1}, LayoutStandard::STD140);
    EXPECT_EQ(info.size, 12u);
    EXPECT_EQ(info.alignment, 16u);  // vec3 aligns to 16 in std140
}

TEST(ShaderLayoutTest, Vec2Layout)
{
    auto info = BufferLayoutCalculator::CalculateType(
        {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 2, 1}, LayoutStandard::STD140);
    EXPECT_EQ(info.size, 8u);
    EXPECT_EQ(info.alignment, 8u);
}

TEST(ShaderLayoutTest, Matrix4x4Layout)
{
    auto info = BufferLayoutCalculator::CalculateType(
        {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}, LayoutStandard::STD140);
    EXPECT_EQ(info.size, 64u);
    EXPECT_EQ(info.alignment, 16u);
}

TEST(ShaderLayoutTest, PassInfoCBufferLayout)
{
    // passInfo: LightMatrix(4x4) + MainLightColor(vec4) + MainLightDirection(vec4) + Viewport(vec4)
    static constexpr MemberDecl members[] = {
        {"LightMatrix",       {ShaderBaseType::FLOAT, ShaderDataType::MATRIX, 4, 4}},
        {"MainLightColor",    {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"MainLightDirection",{ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"Viewport",          {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };

    std::vector<LayoutInfo> layouts;
    uint32_t totalSize = BufferLayoutCalculator::CalculateMembers(members, nullptr, LayoutStandard::STD140, layouts);

    EXPECT_EQ(layouts[0].offset, 0u);   // LightMatrix
    EXPECT_EQ(layouts[0].size, 64u);
    EXPECT_EQ(layouts[1].offset, 64u);  // MainLightColor
    EXPECT_EQ(layouts[2].offset, 80u);  // MainLightDirection
    EXPECT_EQ(layouts[3].offset, 96u);  // Viewport
    EXPECT_EQ(totalSize, 112u);
}

TEST(ShaderLayoutTest, PaddingBetweenScalarAndVec4)
{
    // uint + float4 -> gap of 12 bytes
    static constexpr MemberDecl members[] = {
        {"Flag",   {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {"Offset", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };

    std::vector<LayoutInfo> layouts;
    uint32_t totalSize = BufferLayoutCalculator::CalculateMembers(members, nullptr, LayoutStandard::STD140, layouts);

    EXPECT_EQ(layouts[0].offset, 0u);
    EXPECT_EQ(layouts[0].size, 4u);
    EXPECT_EQ(layouts[1].offset, 16u);  // aligned to 16
    EXPECT_EQ(totalSize, 32u);
}

TEST(ShaderLayoutTest, ScalarArrayStd140Stride)
{
    // float[4] in std140: each element =16 bytes stride
    static constexpr MemberDecl members[] = {
        {"values", {ShaderBaseType::FLOAT, ShaderDataType::SCALAR}, 4},
    };

    std::vector<LayoutInfo> layouts;
    uint32_t totalSize = BufferLayoutCalculator::CalculateMembers(members, nullptr, LayoutStandard::STD140, layouts);

    EXPECT_EQ(layouts[0].stride, 16u);         // each element stride=16
    EXPECT_EQ(layouts[0].size, 16u * 4);       // total = 64
    EXPECT_EQ(totalSize, 64u);
}

TEST(ShaderLayoutTest, ScalarArrayStd430Stride)
{
    // float[4] in std430: each element = 4 bytes stride
    static constexpr MemberDecl members[] = {
        {"values", {ShaderBaseType::FLOAT, ShaderDataType::SCALAR}, 4},
    };

    std::vector<LayoutInfo> layouts;
    uint32_t totalSize = BufferLayoutCalculator::CalculateMembers(members, nullptr, LayoutStandard::STD430, layouts);

    EXPECT_EQ(layouts[0].stride, 4u);
    EXPECT_EQ(layouts[0].size, 16u);
    EXPECT_EQ(totalSize, 16u);
}

TEST(ShaderLayoutTest, MeshletStructSize)
{
    auto group = BuildDefaultLocalGroup();

    const auto *meshlet = FindStructInGroup("Meshlet", group);
    ASSERT_NE(meshlet, nullptr);

    uint32_t sz = BufferLayoutCalculator::CalculateStructSize(*meshlet, &group);
    EXPECT_EQ(sz, 64u);  // 4*uint(4B) + 3*vec4(16B) = 16 + 48 = 64
}

TEST(ShaderLayoutTest, ViewInfoStructSizeStd140)
{
    auto group = BuildDefaultPassGroup();

    const auto *viewInfo = FindStructInGroup("ViewInfo", group);
    ASSERT_NE(viewInfo, nullptr);

    uint32_t sz = BufferLayoutCalculator::CalculateStructSize(*viewInfo, &group);
    EXPECT_EQ(sz, 256u);  // 4 * float4x4(64B) = 256, already 16-aligned
}

TEST(ShaderLayoutTest, ValidationVec3Warning)
{
    static constexpr MemberDecl members[] = {
        {"direction", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 3, 1}},
    };
    ResourceDecl cb = {.type = ResourceType::CONSTANT_BUFFER, .name = "test", .members = members};

    auto messages = BufferLayoutCalculator::Validate(cb);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].level, BufferLayoutCalculator::ValidationMessage::Level::WARNING);
    EXPECT_NE(messages[0].message.find("3-component"), std::string::npos);
}

TEST(ShaderLayoutTest, ValidationScalarArrayWarning)
{
    static constexpr MemberDecl members[] = {
        {"data", {ShaderBaseType::FLOAT, ShaderDataType::SCALAR}, 4},
    };
    ResourceDecl cb = {.type = ResourceType::CONSTANT_BUFFER, .name = "test", .members = members};

    auto messages = BufferLayoutCalculator::Validate(cb);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_NE(messages[0].message.find("scalar array"), std::string::npos);
}

// ---------------------------------------------
//  HLSL Generator Tests
// ---------------------------------------------

TEST(ShaderLayoutTest, HLSLGeneratorDefaultPass)
{
    auto group = BuildDefaultPassGroup();

    ResourceDeclGenerator gen;
    auto hlsl = gen.Generate(group, ShaderLanguage::HLSL);

    // Check header comment
    EXPECT_NE(hlsl.find("AUTO-GENERATED"), std::string::npos);
    EXPECT_NE(hlsl.find("DefaultPass"), std::string::npos);

    // Check struct declaration
    EXPECT_NE(hlsl.find("struct ViewInfo"), std::string::npos);
    EXPECT_NE(hlsl.find("float4x4 World;"), std::string::npos);

    // Check cbuffer declarations
    EXPECT_NE(hlsl.find("[[vk::binding(0, 0)]]"), std::string::npos);
    EXPECT_NE(hlsl.find("cbuffer passInfo : register(b0, space0)"), std::string::npos);
    EXPECT_NE(hlsl.find("float4x4 LightMatrix;"), std::string::npos);

    EXPECT_NE(hlsl.find("[[vk::binding(1, 0)]]"), std::string::npos);
    EXPECT_NE(hlsl.find("cbuffer viewInfo : register(b1, space0)"), std::string::npos);
    EXPECT_NE(hlsl.find("ViewInfo View;"), std::string::npos);

    // Check texture/sampler pairs
    EXPECT_NE(hlsl.find("Texture2D ShadowMap : register(t0, space0)"), std::string::npos);
    EXPECT_NE(hlsl.find("SamplerState ShadowMapSampler : register(s0, space0)"), std::string::npos);
    EXPECT_NE(hlsl.find("TextureCube IrradianceMap : register(t2, space0)"), std::string::npos);
    EXPECT_NE(hlsl.find("TextureCube PrefilteredMap : register(t3, space0)"), std::string::npos);
    EXPECT_NE(hlsl.find("Texture2D HizBuffer : register(t4, space0)"), std::string::npos);
}

TEST(ShaderLayoutTest, HLSLGeneratorDefaultLocal)
{
    auto group = BuildDefaultLocalGroup();

    ResourceDeclGenerator gen;
    auto hlsl = gen.Generate(group, ShaderLanguage::HLSL);

    // Check unconditional cbuffer
    EXPECT_NE(hlsl.find("cbuffer Local : register(b0, space2)"), std::string::npos);
    EXPECT_NE(hlsl.find("float4x4 World;"), std::string::npos);

    // Check conditional block
    EXPECT_NE(hlsl.find("#if MESH_SHADER"), std::string::npos);
    EXPECT_NE(hlsl.find("#endif // MESH_SHADER"), std::string::npos);

    // Check mesh shader resources inside conditional
    EXPECT_NE(hlsl.find("cbuffer MeshletInfo : register(b1, space2)"), std::string::npos);
    EXPECT_NE(hlsl.find("StructuredBuffer<float4> PositionBuf"), std::string::npos);
    EXPECT_NE(hlsl.find("StructuredBuffer<ExtVertex> ExtBuf"), std::string::npos);
    EXPECT_NE(hlsl.find("StructuredBuffer<uint> VertexIndices"), std::string::npos);
    EXPECT_NE(hlsl.find("StructuredBuffer<Meshlet> Meshlets"), std::string::npos);

    // Check struct declarations
    EXPECT_NE(hlsl.find("struct Meshlet"), std::string::npos);
    EXPECT_NE(hlsl.find("struct ExtVertex"), std::string::npos);
}

TEST(ShaderLayoutTest, SSBONestedStructArray)
{
    // StructuredBuffer<BVHNode>, where BVHNode contains an AABB sub-struct
    static constexpr MemberDecl aabbMembers[] = {
        {"minBound", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
        {"maxBound", {ShaderBaseType::FLOAT, ShaderDataType::VECTOR, 4, 1}},
    };
    static constexpr MemberDecl bvhNodeMembers[] = {
        {"bounds",     {ShaderBaseType::NONE, ShaderDataType::STRUCT}, 0, "AABB"},
        {"leftChild",  {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {"rightChild", {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {"primitiveId",{ShaderBaseType::UINT, ShaderDataType::SCALAR}},
        {"flags",      {ShaderBaseType::UINT, ShaderDataType::SCALAR}},
    };
    static constexpr StructDecl localStructs[] = {
        {"AABB",    aabbMembers,    StructUsage::PLAIN},
        {"BVHNode", bvhNodeMembers, StructUsage::SSBO},
    };
    static constexpr ResourceDecl resources[] = {
        {.type = ResourceType::STRUCTURED_BUFFER, .name = "BVHNodes", .elementStructRef = "BVHNode"},
    };
    ResourceGroupDecl group = {
        "AccelStruct", 0,
        rhi::ShaderStageFlagBit::CS,
        localStructs, resources,
    };

    // -- Verify CollectReferencedStructs finds nested AABB before BVHNode --
    auto refs = CollectReferencedStructs(group);
    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(refs[0]->name, "AABB");    // dependency first
    EXPECT_EQ(refs[1]->name, "BVHNode");

    // -- Verify LayoutCalculator: BVHNode size = AABB(32) + 4*uint(16) = 48 --
    const auto *bvhDecl = FindStructInGroup("BVHNode", group);
    ASSERT_NE(bvhDecl, nullptr);
    uint32_t bvhSize = BufferLayoutCalculator::CalculateStructSize(*bvhDecl, &group);
    EXPECT_EQ(bvhSize, 48u);  // std430: AABB(32) + 4*uint(4) = 48

    const auto *aabbDecl = FindStructInGroup("AABB", group);
    ASSERT_NE(aabbDecl, nullptr);
    uint32_t aabbSize = BufferLayoutCalculator::CalculateStructSize(*aabbDecl, &group);
    EXPECT_EQ(aabbSize, 32u);  // 2 * vec4(16) = 32

    // -- Verify HLSL generation --
    ResourceDeclGenerator gen;
    auto hlsl = gen.Generate(group, ShaderLanguage::HLSL);

    // Both structs emitted
    EXPECT_NE(hlsl.find("struct AABB"), std::string::npos);
    EXPECT_NE(hlsl.find("struct BVHNode"), std::string::npos);
    EXPECT_NE(hlsl.find("AABB bounds;"), std::string::npos);

    // AABB declared before BVHNode (dependency order)
    auto aabbPos = hlsl.find("struct AABB");
    auto bvhPos  = hlsl.find("struct BVHNode");
    EXPECT_LT(aabbPos, bvhPos);

    // StructuredBuffer declaration
    EXPECT_NE(hlsl.find("StructuredBuffer<BVHNode> BVHNodes"), std::string::npos);
}

// ---------------------------------------------
//  ResourceGroupDecl Tests
// ---------------------------------------------

TEST(ShaderLayoutTest, LibraryCollectReferencedStructs)
{
    auto group = BuildDefaultPassGroup();

    auto refs = CollectReferencedStructs(group);
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0]->name, "ViewInfo");
}

TEST(ShaderLayoutTest, LibraryCollectReferencedStructsWithConditionals)
{
    auto group = BuildDefaultLocalGroup();

    auto refs = CollectReferencedStructs(group);
    // Should find Meshlet and ExtVertex from conditional block
    EXPECT_GE(refs.size(), 2u);

    bool foundMeshlet = false;
    bool foundExtVertex = false;
    for (const auto *s : refs) {
        if (s->name == "Meshlet") foundMeshlet = true;
        if (s->name == "ExtVertex") foundExtVertex = true;
    }
    EXPECT_TRUE(foundMeshlet);
    EXPECT_TRUE(foundExtVertex);
}

// ---------------------------------------------
//  RHILayoutGenerator Tests
// ---------------------------------------------

TEST(ShaderLayoutTest, RHILayoutDefaultPassBindings)
{
    auto group = BuildDefaultPassGroup();

    auto desc = RHILayoutGenerator::Generate(group);

    // DefaultPass: 2 cbuffers + 5*(texture+sampler) = 12 bindings total
    ASSERT_EQ(desc.bindings.size(), 12u);

    // Verify flat binding numbers (same as [[vk::binding]])
    // passInfo cbuffer -> binding 0
    EXPECT_EQ(desc.bindings[0].binding, 0u);
    EXPECT_EQ(desc.bindings[0].type, rhi::DescriptorType::UNIFORM_BUFFER);
    EXPECT_EQ(desc.bindings[0].name, "passInfo");

    // viewInfo cbuffer -> binding 1
    EXPECT_EQ(desc.bindings[1].binding, 1u);
    EXPECT_EQ(desc.bindings[1].type, rhi::DescriptorType::UNIFORM_BUFFER);
    EXPECT_EQ(desc.bindings[1].name, "viewInfo");

    // ShadowMap texture -> binding 2
    EXPECT_EQ(desc.bindings[2].binding, 2u);
    EXPECT_EQ(desc.bindings[2].type, rhi::DescriptorType::SAMPLED_IMAGE);
    EXPECT_EQ(desc.bindings[2].name, "ShadowMap");

    // ShadowMapSampler -> binding 3
    EXPECT_EQ(desc.bindings[3].binding, 3u);
    EXPECT_EQ(desc.bindings[3].type, rhi::DescriptorType::SAMPLER);
    EXPECT_EQ(desc.bindings[3].name, "ShadowMapSampler");

    // BRDFLut -> binding 4
    EXPECT_EQ(desc.bindings[4].binding, 4u);
    EXPECT_EQ(desc.bindings[4].type, rhi::DescriptorType::SAMPLED_IMAGE);

    // Last: HizBufferSampler -> binding 11
    EXPECT_EQ(desc.bindings[11].binding, 11u);
    EXPECT_EQ(desc.bindings[11].type, rhi::DescriptorType::SAMPLER);
    EXPECT_EQ(desc.bindings[11].name, "HizBufferSampler");

    // All should have default visibility from group
    auto expectedVis = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS;
    for (const auto &b : desc.bindings) {
        EXPECT_EQ(b.visibility, expectedVis);
    }
}

TEST(ShaderLayoutTest, RHILayoutDefaultLocalSuperset)
{
    auto group = BuildDefaultLocalGroup();

    // Generate with all conditionals (superset)
    auto desc = RHILayoutGenerator::Generate(group);

    // Local cbuffer (binding 0) + MESH_SHADER block: MeshletInfo(1) + 6 StructuredBuffers(2-7)
    // = 1 unconditional + 7 conditional = 8 total
    ASSERT_EQ(desc.bindings.size(), 8u);

    // Local cbuffer -> binding 0, DYNAMIC
    EXPECT_EQ(desc.bindings[0].binding, 0u);
    EXPECT_EQ(desc.bindings[0].type, rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC);
    EXPECT_EQ(desc.bindings[0].name, "Local");

    // MeshletInfo -> binding 1, DYNAMIC
    EXPECT_EQ(desc.bindings[1].binding, 1u);
    EXPECT_EQ(desc.bindings[1].type, rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC);
    EXPECT_EQ(desc.bindings[1].name, "MeshletInfo");

    // PositionBuf StructuredBuffer -> binding 2, STORAGE_BUFFER
    EXPECT_EQ(desc.bindings[2].binding, 2u);
    EXPECT_EQ(desc.bindings[2].type, rhi::DescriptorType::STORAGE_BUFFER);
    EXPECT_EQ(desc.bindings[2].name, "PositionBuf");

    // InstanceBuffer -> binding 7
    EXPECT_EQ(desc.bindings[7].binding, 7u);
    EXPECT_EQ(desc.bindings[7].type, rhi::DescriptorType::STORAGE_BUFFER);
    EXPECT_EQ(desc.bindings[7].name, "InstanceBuffer");
}

TEST(ShaderLayoutTest, RHILayoutDefaultLocalBaseOnly)
{
    auto group = BuildDefaultLocalGroup();

    // Generate with NO conditions active (base layout only)
    auto desc = RHILayoutGenerator::GenerateWithConditions(group, {});

    // Only unconditional: Local cbuffer
    ASSERT_EQ(desc.bindings.size(), 1u);
    EXPECT_EQ(desc.bindings[0].binding, 0u);
    EXPECT_EQ(desc.bindings[0].name, "Local");
}

TEST(ShaderLayoutTest, RHILayoutDefaultLocalWithMeshShader)
{
    auto group = BuildDefaultLocalGroup();

    // Generate with MESH_SHADER active
    auto desc = RHILayoutGenerator::GenerateWithConditions(
        group, {"MESH_SHADER"});

    // 1 unconditional + 7 conditional = 8
    ASSERT_EQ(desc.bindings.size(), 8u);
}

TEST(ShaderLayoutTest, RHILayoutMatchesManualDefaultPass)
{
    // Verify that generated layout matches what DefaultForwardPipeline.cpp
    // manually creates -- binding numbers should be identical
    auto group = BuildDefaultPassGroup();

    auto desc = RHILayoutGenerator::Generate(group);

    // Manual layout from DefaultForwardPipeline.cpp uses flat bindings:
    //   passInfo=0, viewInfo=1, ShadowMap=2, ShadowMapSampler=3,
    //   BRDFLut=4, BRDFLutSampler=5, IrradianceMap=6, IrradianceSampler=7,
    //   PrefilteredMap=8, PrefilteredMapSampler=9, HizBuffer=10, HizBufferSampler=11
    const uint32_t expectedBindings[] = {0,1,2,3,4,5,6,7,8,9,10,11};
    ASSERT_EQ(desc.bindings.size(), 12u);
    for (uint32_t i = 0; i < 12; ++i) {
        EXPECT_EQ(desc.bindings[i].binding, expectedBindings[i]);
    }
}
