//
// Created on 2026/09/21.
//

#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

#include <vector>

using namespace sky::ui;

TEST(UIPaintContextTest, SingleRectEmitsOneCommand)
{
    UIPaintContext ctx;
    ctx.Begin(UIRect{0.0f, 0.0f, 100.0f, 100.0f});
    ctx.AddRect(UIRect{10.0f, 10.0f, 50.0f, 30.0f}, 0xFF112233);

    const UIDrawData &data = ctx.GetDrawData();
    ASSERT_EQ(data.vertices.size(), 4u);
    ASSERT_EQ(data.indices.size(), 6u);
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_EQ(data.commands[0].indexOffset, 0u);
    EXPECT_EQ(data.commands[0].indexCount, 6u);
    EXPECT_EQ(data.vertices[0].x, 10.0f);
    EXPECT_EQ(data.vertices[0].y, 10.0f);
    EXPECT_EQ(data.vertices[0].color, 0xFF112233u);
}

TEST(UIPaintContextTest, SameTextureAndClipMerge)
{
    UIPaintContext ctx;
    ctx.Begin(UIRect{0.0f, 0.0f, 100.0f, 100.0f});
    ctx.AddRect(UIRect{0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFF);
    ctx.AddRect(UIRect{20.0f, 20.0f, 30.0f, 30.0f}, 0xFFFFFFFF);

    const UIDrawData &data = ctx.GetDrawData();
    ASSERT_EQ(data.vertices.size(), 8u);
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_EQ(data.commands[0].indexOffset, 0u);
    EXPECT_EQ(data.commands[0].indexCount, 12u);
}

// Regression: indexOffset must be the INDEX base, not the vertex base. It used to
// be set to drawData.vertices.size(), which broke every command after the first.
TEST(UIPaintContextTest, IndexOffsetUsesIndexBaseNotEmptyVertexBase)
{
    UIPaintContext ctx;
    ctx.Begin(UIRect{0.0f, 0.0f, 100.0f, 100.0f});
    ctx.AddRect(UIRect{0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFF);
    ctx.AddTexturedQuad(UIRect{20.0f, 20.0f, 40.0f, 40.0f}, UIRect{0.0f, 0.0f, 1.0f, 1.0f}, 7, 0xFFFFFFFF);

    const UIDrawData &data = ctx.GetDrawData();
    ASSERT_EQ(data.commands.size(), 2u);
    EXPECT_EQ(data.commands[0].indexOffset, 0u);
    EXPECT_EQ(data.commands[0].indexCount, 6u);

    // 4 vertices were emitted (vertex base = 4), but the second command's indices
    // start at index 6. The old bug set indexOffset = 4.
    EXPECT_EQ(data.commands[1].indexOffset, 6u);
    EXPECT_EQ(data.commands[1].indexCount, 6u);
    EXPECT_EQ(data.commands[1].textureId, 7u);

    ASSERT_EQ(data.indices.size(), 12u);
    EXPECT_EQ(data.indices[6], 4u);
    EXPECT_EQ(data.indices[7], 5u);
    EXPECT_EQ(data.indices[8], 6u);
    EXPECT_EQ(data.indices[9], 4u);
    EXPECT_EQ(data.indices[10], 6u);
    EXPECT_EQ(data.indices[11], 7u);
}

TEST(UIPaintContextTest, ClipChangeStartsNewCommand)
{
    UIPaintContext ctx;
    ctx.Begin(UIRect{0.0f, 0.0f, 100.0f, 100.0f});
    ctx.AddRect(UIRect{0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFF);
    ctx.PushClip(UIRect{0.0f, 0.0f, 50.0f, 50.0f});
    ctx.AddRect(UIRect{0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFF);

    const UIDrawData &data = ctx.GetDrawData();
    ASSERT_EQ(data.commands.size(), 2u);
    EXPECT_EQ(data.commands[1].indexOffset, 6u);
    EXPECT_EQ(data.commands[1].clip.right, 50.0f);
    ctx.PopClip();
}

TEST(UIPaintContextTest, TexturedQuadCarriesTextureId)
{
    UIPaintContext ctx;
    ctx.Begin(UIRect{0.0f, 0.0f, 100.0f, 100.0f});
    ctx.AddTexturedQuad(UIRect{0.0f, 0.0f, 64.0f, 64.0f}, UIRect{0.25f, 0.25f, 0.5f, 0.5f}, 3, 0x80FFFFFF);

    const UIDrawData &data = ctx.GetDrawData();
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_EQ(data.commands[0].textureId, 3u);
    EXPECT_EQ(data.vertices[0].u, 0.25f);
    EXPECT_EQ(data.vertices[2].v, 0.5f);
}
