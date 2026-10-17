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

TEST(UIPaintContextTest, NestedClipsUseTheirAccumulatedSurfaceTransforms)
{
    UIPaintContext ctx;
    ctx.Begin({0.0f, 0.0f, 200.0f, 100.0f});

    UI2DTransform parent;
    parent.m02 = 50.0f;
    ctx.PushTransform(parent);
    ctx.PushClip({0.0f, 0.0f, 100.0f, 100.0f});
    EXPECT_FLOAT_EQ(ctx.CurrentClip().left, 50.0f);
    EXPECT_FLOAT_EQ(ctx.CurrentClip().right, 150.0f);

    UI2DTransform child;
    child.m02 = 40.0f;
    ctx.PushTransform(child);
    ctx.PushClip({0.0f, 0.0f, 80.0f, 100.0f});
    ctx.AddRect({0.0f, 0.0f, 80.0f, 100.0f}, 0xFFFFFFFF);

    ASSERT_EQ(ctx.GetDrawData().commands.size(), 1u);
    EXPECT_FLOAT_EQ(ctx.GetDrawData().commands[0].clip.left, 90.0f);
    EXPECT_FLOAT_EQ(ctx.GetDrawData().commands[0].clip.right, 150.0f);
    ctx.PopClip();
    ctx.PopTransform();
    EXPECT_FLOAT_EQ(ctx.CurrentClip().left, 50.0f);
    ctx.PopClip();
    ctx.PopTransform();
    EXPECT_FLOAT_EQ(ctx.CurrentClip().left, 0.0f);
    EXPECT_FLOAT_EQ(ctx.CurrentClip().right, 200.0f);
}

TEST(UIPaintContextTest, CullUsesTransformedBounds)
{
    UIPaintContext ctx;
    ctx.Begin({0.0f, 0.0f, 100.0f, 100.0f});
    UI2DTransform moveIntoSurface;
    moveIntoSurface.m02 = -100.0f;
    ctx.PushTransform(moveIntoSurface);
    ctx.AddRect({110.0f, 10.0f, 130.0f, 30.0f}, 0xFFFFFFFF);

    ASSERT_EQ(ctx.GetDrawData().vertices.size(), 4u);
    EXPECT_FLOAT_EQ(ctx.GetDrawData().vertices[0].x, 10.0f);
    EXPECT_FLOAT_EQ(ctx.GetDrawData().vertices[2].x, 30.0f);

    ctx.Begin({0.0f, 0.0f, 100.0f, 100.0f});
    UI2DTransform moveOutsideSurface;
    moveOutsideSurface.m02 = 100.0f;
    ctx.PushTransform(moveOutsideSurface);
    ctx.AddRect({10.0f, 10.0f, 30.0f, 30.0f}, 0xFFFFFFFF);
    EXPECT_TRUE(ctx.GetDrawData().commands.empty());
    EXPECT_TRUE(ctx.GetDrawData().vertices.empty());
}

TEST(UIPaintContextTest, RotatedClipUsesAllFourCorners)
{
    UIPaintContext ctx;
    ctx.Begin({0.0f, 0.0f, 100.0f, 100.0f});
    UI2DTransform rotate;
    rotate.m00 = 0.0f;
    rotate.m01 = -1.0f;
    rotate.m02 = 80.0f;
    rotate.m10 = 1.0f;
    rotate.m11 = 0.0f;
    ctx.PushTransform(rotate);
    ctx.PushClip({10.0f, 20.0f, 30.0f, 40.0f});
    ctx.AddRect({10.0f, 20.0f, 30.0f, 40.0f}, 0xFFFFFFFF);

    ASSERT_EQ(ctx.GetDrawData().commands.size(), 1u);
    const UIRect &clip = ctx.GetDrawData().commands[0].clip;
    EXPECT_FLOAT_EQ(clip.left, 40.0f);
    EXPECT_FLOAT_EQ(clip.top, 10.0f);
    EXPECT_FLOAT_EQ(clip.right, 60.0f);
    EXPECT_FLOAT_EQ(clip.bottom, 30.0f);
}

TEST(UIPaintContextTest, RoundedShapeCarriesLayoutCoordinatesThroughAffineTransform)
{
    UIPaintContext ctx;
    ctx.Begin({0.0f, 0.0f, 500.0f, 500.0f});
    UI2DTransform transform;
    transform.m00 = 0.0f;
    transform.m01 = -2.0f;
    transform.m02 = 200.0f;
    transform.m10 = 3.0f;
    transform.m11 = 0.0f;
    transform.m12 = 20.0f;
    ctx.PushTransform(transform);
    ctx.AddRoundedRect({10.0f, 20.0f, 30.0f, 40.0f}, 0xFFFFFFFF, 4.0f);

    const UIDrawData &data = ctx.GetDrawData();
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_TRUE(data.commands[0].shape);
    ASSERT_EQ(data.vertices.size(), 4u);
    const float layoutX[] = {10.0f, 30.0f, 30.0f, 10.0f};
    const float layoutY[] = {20.0f, 20.0f, 40.0f, 40.0f};
    for (size_t i = 0; i < data.vertices.size(); ++i) {
        const UIVertex &vertex = data.vertices[i];
        EXPECT_FLOAT_EQ(vertex.x, 200.0f - 2.0f * layoutY[i]);
        EXPECT_FLOAT_EQ(vertex.y, 20.0f + 3.0f * layoutX[i]);
        EXPECT_FLOAT_EQ(vertex.u, layoutX[i]);
        EXPECT_FLOAT_EQ(vertex.v, layoutY[i]);
        EXPECT_FLOAT_EQ(vertex.roundCenterX, 20.0f);
        EXPECT_FLOAT_EQ(vertex.roundCenterY, 30.0f);
        EXPECT_FLOAT_EQ(vertex.roundHalfX, 10.0f);
        EXPECT_FLOAT_EQ(vertex.roundHalfY, 10.0f);
        EXPECT_FLOAT_EQ(vertex.roundRadius, 4.0f);
    }

    ctx.AddTexturedQuad({10.0f, 20.0f, 30.0f, 40.0f}, {0.25f, 0.5f, 0.75f, 1.0f}, 7, 0xFFFFFFFF);
    ASSERT_EQ(data.commands.size(), 2u);
    EXPECT_FALSE(data.commands[1].shape);
    ASSERT_EQ(data.vertices.size(), 8u);
    EXPECT_FLOAT_EQ(data.vertices[4].u, 0.25f);
    EXPECT_FLOAT_EQ(data.vertices[4].v, 0.5f);
    EXPECT_FLOAT_EQ(data.vertices[6].u, 0.75f);
    EXPECT_FLOAT_EQ(data.vertices[6].v, 1.0f);
}
