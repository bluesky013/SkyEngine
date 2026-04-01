//
// Created on 2026/04/01.
//

#include <gtest/gtest.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/PipelineState.h>

using namespace sky::aurora;

// ---------------------------------------------------------------------------
// AttachmentFormat struct tests (no device required)
// ---------------------------------------------------------------------------
TEST(AttachmentFormatTest, DefaultValues)
{
    AttachmentFormat fmt = {};
    EXPECT_EQ(fmt.numColors, 0u);
    EXPECT_EQ(fmt.depthStencil, PixelFormat::UNDEFINED);
    EXPECT_EQ(fmt.sampleCount, SampleCount::X1);
    EXPECT_EQ(fmt.viewMask, 0u);

    for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; ++i) {
        EXPECT_EQ(fmt.colors[i], PixelFormat::UNDEFINED);
    }
}

TEST(AttachmentFormatTest, SingleColor)
{
    AttachmentFormat fmt = {};
    fmt.colors[0] = PixelFormat::RGBA8_UNORM;
    fmt.numColors = 1;

    EXPECT_EQ(fmt.colors[0], PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(fmt.numColors, 1u);
}

TEST(AttachmentFormatTest, MultipleColorAttachments)
{
    AttachmentFormat fmt = {};
    fmt.colors[0] = PixelFormat::RGBA8_UNORM;
    fmt.colors[1] = PixelFormat::RGBA16_SFLOAT;
    fmt.colors[2] = PixelFormat::R32_SFLOAT;
    fmt.numColors = 3;
    fmt.depthStencil = PixelFormat::D24_S8;

    EXPECT_EQ(fmt.numColors, 3u);
    EXPECT_EQ(fmt.colors[0], PixelFormat::RGBA8_UNORM);
    EXPECT_EQ(fmt.colors[1], PixelFormat::RGBA16_SFLOAT);
    EXPECT_EQ(fmt.colors[2], PixelFormat::R32_SFLOAT);
    EXPECT_EQ(fmt.depthStencil, PixelFormat::D24_S8);
}

TEST(AttachmentFormatTest, MSAA)
{
    AttachmentFormat fmt = {};
    fmt.colors[0]   = PixelFormat::RGBA8_UNORM;
    fmt.numColors   = 1;
    fmt.depthStencil = PixelFormat::D32;
    fmt.sampleCount = SampleCount::X4;

    EXPECT_EQ(fmt.sampleCount, SampleCount::X4);
}

TEST(AttachmentFormatTest, Multiview)
{
    AttachmentFormat fmt = {};
    fmt.colors[0] = PixelFormat::RGBA8_UNORM;
    fmt.numColors = 1;
    fmt.viewMask  = 0x3; // 2 views

    EXPECT_EQ(fmt.viewMask, 0x3u);
}

TEST(AttachmentFormatTest, MaxColorAttachments)
{
    EXPECT_EQ(MAX_COLOR_ATTACHMENTS, 8u);
}

// ---------------------------------------------------------------------------
// PipelineState struct tests
// ---------------------------------------------------------------------------
TEST(PipelineStateTest, DefaultDepthStencil)
{
    DepthStencil ds = {};
    EXPECT_FALSE(ds.depthTest);
    EXPECT_FALSE(ds.depthWrite);
    EXPECT_FALSE(ds.stencilTest);
    EXPECT_EQ(ds.compareOp, CompareOp::LESS_OR_EQUAL);
}

TEST(PipelineStateTest, DefaultRasterState)
{
    RasterState rs = {};
    EXPECT_FALSE(rs.depthClampEnable);
    EXPECT_FALSE(rs.rasterizerDiscardEnable);
    EXPECT_FALSE(rs.depthBiasEnable);
    EXPECT_EQ(rs.frontFace, FrontFace::CCW);
    EXPECT_EQ(rs.polygonMode, PolygonMode::FILL);
    EXPECT_FLOAT_EQ(rs.lineWidth, 1.f);
}

TEST(PipelineStateTest, DefaultBlendState)
{
    BlendState bs = {};
    EXPECT_FALSE(bs.blendEn);
    EXPECT_EQ(bs.writeMask, 0xF);
    EXPECT_EQ(bs.srcColor, BlendFactor::ZERO);
    EXPECT_EQ(bs.dstColor, BlendFactor::ZERO);
    EXPECT_EQ(bs.colorBlendOp, BlendOp::ADD);
    EXPECT_EQ(bs.alphaBlendOp, BlendOp::ADD);
}

TEST(PipelineStateTest, DefaultInputAssembly)
{
    InputAssembly ia = {};
    EXPECT_EQ(ia.topology, PrimitiveTopology::TRIANGLE_LIST);
}

TEST(PipelineStateTest, DefaultMultiSample)
{
    MultiSample ms = {};
    EXPECT_FALSE(ms.alphaToCoverage);
    EXPECT_EQ(ms.sampleCount, SampleCount::X1);
}

TEST(PipelineStateTest, BlendStateAlphaBlend)
{
    BlendState bs = {};
    bs.blendEn  = true;
    bs.srcColor = BlendFactor::SRC_ALPHA;
    bs.dstColor = BlendFactor::ONE_MINUS_SRC_ALPHA;
    bs.srcAlpha = BlendFactor::ONE;
    bs.dstAlpha = BlendFactor::ZERO;
    bs.colorBlendOp = BlendOp::ADD;
    bs.alphaBlendOp = BlendOp::ADD;
    bs.writeMask    = 0xF;

    EXPECT_TRUE(bs.blendEn);
    EXPECT_EQ(bs.srcColor, BlendFactor::SRC_ALPHA);
    EXPECT_EQ(bs.dstColor, BlendFactor::ONE_MINUS_SRC_ALPHA);
}

TEST(PipelineStateTest, DepthStencilEnabled)
{
    DepthStencil ds = {};
    ds.depthTest  = true;
    ds.depthWrite = true;
    ds.compareOp  = CompareOp::LESS;
    ds.stencilTest = true;
    ds.front.failOp      = StencilOp::KEEP;
    ds.front.passOp      = StencilOp::REPLACE;
    ds.front.depthFailOp = StencilOp::KEEP;
    ds.front.compareOp   = CompareOp::ALWAYS;
    ds.front.compareMask = 0xFF;
    ds.front.writeMask   = 0xFF;
    ds.front.reference   = 1;

    EXPECT_TRUE(ds.depthTest);
    EXPECT_TRUE(ds.depthWrite);
    EXPECT_TRUE(ds.stencilTest);
    EXPECT_EQ(ds.front.passOp, StencilOp::REPLACE);
}

// ---------------------------------------------------------------------------
// Struct size / enum range sanity
// ---------------------------------------------------------------------------
TEST(CoreEnumTest, PixelFormatRange)
{
    EXPECT_EQ(static_cast<uint32_t>(PixelFormat::UNDEFINED), 0u);
    EXPECT_GT(static_cast<uint32_t>(PixelFormat::MAX), 50u);
}

TEST(CoreEnumTest, CompareOpRange)
{
    EXPECT_EQ(static_cast<uint32_t>(CompareOp::NEVER), 0u);
    EXPECT_EQ(static_cast<uint32_t>(CompareOp::ALWAYS), 7u);
}

TEST(CoreEnumTest, BlendFactorRange)
{
    EXPECT_EQ(static_cast<uint32_t>(BlendFactor::ZERO), 0u);
    EXPECT_EQ(static_cast<uint32_t>(BlendFactor::ONE_MINUS_SRC1_ALPHA), 18u);
}

TEST(CoreEnumTest, SampleCountValues)
{
    EXPECT_EQ(static_cast<uint32_t>(SampleCount::X1), 1u);
    EXPECT_EQ(static_cast<uint32_t>(SampleCount::X2), 2u);
    EXPECT_EQ(static_cast<uint32_t>(SampleCount::X4), 4u);
    EXPECT_EQ(static_cast<uint32_t>(SampleCount::X8), 8u);
}

TEST(CoreEnumTest, StencilOpRange)
{
    EXPECT_EQ(static_cast<uint32_t>(StencilOp::KEEP), 0u);
    EXPECT_EQ(static_cast<uint32_t>(StencilOp::DECREMENT_AND_WRAP), 7u);
}

TEST(CoreEnumTest, PrimitiveTopologyRange)
{
    EXPECT_EQ(static_cast<uint32_t>(PrimitiveTopology::POINT_LIST), 0u);
    EXPECT_EQ(static_cast<uint32_t>(PrimitiveTopology::TRIANGLE_FAN), 5u);
}

// ---------------------------------------------------------------------------
// ClearValue construction
// ---------------------------------------------------------------------------
TEST(ClearValueTest, FloatColor)
{
    ClearValue cv(0.1f, 0.2f, 0.3f, 1.0f);
    EXPECT_FLOAT_EQ(cv.color.float32[0], 0.1f);
    EXPECT_FLOAT_EQ(cv.color.float32[1], 0.2f);
    EXPECT_FLOAT_EQ(cv.color.float32[2], 0.3f);
    EXPECT_FLOAT_EQ(cv.color.float32[3], 1.0f);
}

TEST(ClearValueTest, UintColor)
{
    ClearValue cv(1u, 2u, 3u, 255u);
    EXPECT_EQ(cv.color.uint32[0], 1u);
    EXPECT_EQ(cv.color.uint32[3], 255u);
}

TEST(ClearValueTest, DepthStencilClear)
{
    ClearValue cv(1.0f, 0u);
    EXPECT_FLOAT_EQ(cv.depthStencil.depth, 1.0f);
    EXPECT_EQ(cv.depthStencil.stencil, 0u);
}

// ---------------------------------------------------------------------------
// Flag bit operations
// ---------------------------------------------------------------------------
TEST(FlagTest, BufferUsageCombine)
{
    auto flags = BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX;
    EXPECT_TRUE(flags & BufferUsageFlagBit::VERTEX);
    EXPECT_TRUE(flags & BufferUsageFlagBit::INDEX);
    EXPECT_FALSE(flags & BufferUsageFlagBit::UNIFORM);
}

TEST(FlagTest, ImageUsageCombine)
{
    auto flags = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::RENDER_TARGET;
    EXPECT_TRUE(flags & ImageUsageFlagBit::SAMPLED);
    EXPECT_TRUE(flags & ImageUsageFlagBit::RENDER_TARGET);
    EXPECT_FALSE(flags & ImageUsageFlagBit::DEPTH_STENCIL);
}

TEST(FlagTest, ShaderStageCombine)
{
    auto flags = ShaderStageFlagBit::VS | ShaderStageFlagBit::FS;
    EXPECT_TRUE(flags & ShaderStageFlagBit::VS);
    EXPECT_TRUE(flags & ShaderStageFlagBit::FS);
    EXPECT_FALSE(flags & ShaderStageFlagBit::CS);
}
