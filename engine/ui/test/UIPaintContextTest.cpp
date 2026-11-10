//
// Created on 2026/09/21.
//

#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace sky::ui;

TEST(UIPaintContextTest, TriangleClipsGeometryAndPreservesCoverage)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 200, 200});
    paint.PushClip({20, 30, 120, 130});
    paint.AddTriangle(-100000, -100000, 100000, -100000, 0, 100000, 0xFFFFFFFF);
    const auto &data = paint.GetDrawData();
    ASSERT_FALSE(data.vertices.empty());
    double area = 0;
    for (const auto &vertex : data.vertices) {
        ASSERT_GE(vertex.x, 20);
        ASSERT_LE(vertex.x, 120);
        ASSERT_GE(vertex.y, 30);
        ASSERT_LE(vertex.y, 130);
    }
    for (size_t i = 0; i < data.indices.size(); i += 3) {
        const auto &a = data.vertices[data.indices[i]];
        const auto &b = data.vertices[data.indices[i + 1]];
        const auto &c = data.vertices[data.indices[i + 2]];
        area += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) * .5;
    }
    EXPECT_NEAR(area, 10000, .01);
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_EQ(data.commands.front().indexCount, data.indices.size());
}

TEST(UIPaintContextTest, TriangleClipUsesTransformedSurfaceAndBatchesCorrectly)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 200, 200});
    UI2DTransform transform;
    transform.m00 = 2;
    transform.m11 = 2;
    transform.m02 = 20;
    transform.m12 = 30;
    paint.PushTransform(transform);
    paint.PushClip({0, 0, 50, 50});
    paint.PushOpacity(.5f);
    paint.AddTriangle(-100, -100, 200, -100, 0, 200, 0xFFFFFFFF);
    const auto firstIndices = paint.GetDrawData().indices.size();
    paint.AddTriangle(1, 1, 10, 1, 1, 10, 0xFFFFFFFF);
    const auto &data = paint.GetDrawData();
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_EQ(data.commands.front().indexCount, firstIndices + 3);
    for (const auto &vertex : data.vertices) {
        EXPECT_GE(vertex.x, 20);
        EXPECT_LE(vertex.x, 120);
        EXPECT_GE(vertex.y, 30);
        EXPECT_LE(vertex.y, 130);
        EXPECT_EQ(vertex.color, 0x80FFFFFFu);
    }
    paint.AddTexturedQuad({1, 1, 10, 10}, {0, 0, 1, 1}, 9, 0xFFFFFFFF);
    ASSERT_EQ(data.commands.size(), 2u);
    EXPECT_EQ(data.commands.back().indexOffset, firstIndices + 3);
}

TEST(UIPaintContextTest, OffscreenAndEmptyClipTrianglesProduceNoVertices)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 100, 100});
    paint.AddTriangle(10000, 10000, 20000, 10000, 10000, 20000, 0xFFFFFFFF);
    paint.PushClip({200, 200, 300, 300});
    paint.AddTriangle(0, 0, 100, 0, 0, 100, 0xFFFFFFFF);
    EXPECT_TRUE(paint.GetDrawData().vertices.empty());
    EXPECT_TRUE(paint.GetDrawData().indices.empty());
    EXPECT_TRUE(paint.GetDrawData().commands.empty());
}

TEST(UIPaintContextTest, InvalidTriangleStillReachesSnapshotValidation)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 100, 100});
    paint.AddTriangle(std::numeric_limits<float>::quiet_NaN(), 0, 10, 0, 0, 10, 0xFFFFFFFF);
    ASSERT_EQ(paint.GetDrawData().vertices.size(), 3u);
    EXPECT_FALSE(std::isfinite(paint.GetDrawData().vertices.front().x));
}

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

TEST(UIPaintContextTest, RectanglesAndTrianglesShareOneContiguousUntexturedBatch)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 100, 100});
    paint.AddRect({10, 10, 20, 20}, 0xFF112233);
    paint.AddTriangle(30, 30, 40, 30, 30, 40, 0xFF445566);
    paint.AddRect({50, 50, 60, 60}, 0xFF778899);

    const auto &data = paint.GetDrawData();
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_FALSE(data.commands.front().shape);
    EXPECT_EQ(data.commands.front().indexOffset, 0u);
    EXPECT_EQ(data.commands.front().indexCount, 15u);
    ASSERT_EQ(data.vertices.size(), 11u);
    EXPECT_EQ(data.vertices[0].color, 0xFF112233u);
    EXPECT_EQ(data.vertices[4].color, 0xFF445566u);
    EXPECT_EQ(data.vertices[7].color, 0xFF778899u);
    EXPECT_EQ(data.indices, (std::vector<uint32_t>{0, 1, 2, 0, 2, 3, 4, 5, 6, 7, 8, 9, 7, 9, 10}));
}

TEST(UIPaintContextTest, FourChannelsOfNinetySixLinesShareOneShapeBatch)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 500, 500});
    for (uint32_t channel = 0; channel < 4; ++channel) {
        const float y = 10.f + static_cast<float>(channel) * 20.f;
        for (uint32_t segment = 0; segment < 96; ++segment) {
            const float x = 10.f + static_cast<float>(segment);
            paint.AddLine(x, y, x + 1.f, y, static_cast<float>(channel + 1), 0xFF000000u | channel);
        }
    }
    const auto &data = paint.GetDrawData();
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_TRUE(data.commands.front().shape);
    EXPECT_EQ(data.commands.front().textureId, UI_INVALID_TEXTURE);
    EXPECT_EQ(data.commands.front().indexOffset, 0u);
    EXPECT_EQ(data.commands.front().indexCount, 4u * 96u * 6u);
    ASSERT_EQ(data.vertices.size(), 4u * 96u * 4u);
    ASSERT_EQ(data.indices.size(), 4u * 96u * 6u);

    const std::array<uint32_t, 6> quadIndices{0, 1, 2, 0, 2, 3};
    for (uint32_t line = 0; line < 4u * 96u; ++line) {
        const uint32_t             channel = line / 96;
        const float                radius  = static_cast<float>(channel + 1) * 0.5f;
        const float                extent  = 0.5f + radius;
        const float                x       = 10.f + static_cast<float>(line % 96);
        const float                y       = 10.f + static_cast<float>(channel) * 20.f;
        const std::array<float, 4> localX{extent, extent, -extent, -extent};
        const std::array<float, 4> localY{radius, -radius, -radius, radius};
        for (uint32_t corner = 0; corner < 4; ++corner) {
            const auto &vertex = data.vertices[line * 4 + corner];
            EXPECT_FLOAT_EQ(vertex.x, x + 0.5f + localX[corner]);
            EXPECT_FLOAT_EQ(vertex.y, y + localY[corner]);
            EXPECT_FLOAT_EQ(vertex.u, localX[corner]);
            EXPECT_FLOAT_EQ(vertex.v, localY[corner]);
            EXPECT_FLOAT_EQ(vertex.roundCenterX, 0.f);
            EXPECT_FLOAT_EQ(vertex.roundCenterY, 0.f);
            EXPECT_FLOAT_EQ(vertex.roundHalfX, extent);
            EXPECT_FLOAT_EQ(vertex.roundHalfY, radius);
            EXPECT_FLOAT_EQ(vertex.roundRadius, radius);
            EXPECT_EQ(vertex.color, 0xFF000000u | channel);
        }
        for (uint32_t index = 0; index < quadIndices.size(); ++index)
            EXPECT_EQ(data.indices[line * 6 + index], line * 4 + quadIndices[index]);
    }
}

TEST(UIPaintContextTest, ShapeParametersStayPerVertexWhenRoundedRectanglesAndLinesMerge)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 200, 200});
    paint.AddRoundedRect({10, 20, 30, 60}, 0xFF112233, 3.f);
    paint.PushOpacity(0.5f);
    paint.AddLine(50, 50, 70, 50, 8.f, 0xFF445566);
    paint.PopOpacity();
    paint.AddRoundedRect({80, 90, 120, 110}, 0xFF778899, 9.f);

    const auto &data = paint.GetDrawData();
    ASSERT_EQ(data.commands.size(), 1u);
    EXPECT_TRUE(data.commands.front().shape);
    EXPECT_EQ(data.commands.front().indexCount, 18u);
    ASSERT_EQ(data.vertices.size(), 12u);
    const auto &first = data.vertices[0];
    EXPECT_FLOAT_EQ(first.roundCenterX, 20.f);
    EXPECT_FLOAT_EQ(first.roundCenterY, 40.f);
    EXPECT_FLOAT_EQ(first.roundHalfX, 10.f);
    EXPECT_FLOAT_EQ(first.roundHalfY, 20.f);
    EXPECT_FLOAT_EQ(first.roundRadius, 3.f);
    EXPECT_EQ(first.color, 0xFF112233u);
    const auto &line = data.vertices[4];
    EXPECT_FLOAT_EQ(line.roundCenterX, 0.f);
    EXPECT_FLOAT_EQ(line.roundCenterY, 0.f);
    EXPECT_FLOAT_EQ(line.roundHalfX, 14.f);
    EXPECT_FLOAT_EQ(line.roundHalfY, 4.f);
    EXPECT_FLOAT_EQ(line.roundRadius, 4.f);
    EXPECT_EQ(line.color, 0x80445566u);
    const auto &last = data.vertices[8];
    EXPECT_FLOAT_EQ(last.roundCenterX, 100.f);
    EXPECT_FLOAT_EQ(last.roundCenterY, 100.f);
    EXPECT_FLOAT_EQ(last.roundHalfX, 20.f);
    EXPECT_FLOAT_EQ(last.roundHalfY, 10.f);
    EXPECT_FLOAT_EQ(last.roundRadius, 9.f);
    EXPECT_EQ(last.color, 0xFF778899u);
}

TEST(UIPaintContextTest, ShapeTextureAndClipChangesKeepDrawOrderAndSeparateBatches)
{
    UIPaintContext paint;
    paint.Begin({0, 0, 200, 200});
    paint.AddRoundedRect({10, 10, 20, 20}, 0xFFFFFFFF, 2.f);
    paint.AddLine(10, 30, 20, 30, 2.f, 0xFFFFFFFF);
    paint.AddRect({30, 10, 40, 20}, 0xFFFFFFFF);
    paint.AddTriangle(30, 30, 40, 30, 30, 40, 0xFFFFFFFF);
    paint.AddTexturedQuad({50, 10, 60, 20}, {0, 0, 1, 1}, 7, 0xFFFFFFFF);
    paint.AddTexturedQuad({50, 30, 60, 40}, {0, 0, 1, 1}, 7, 0xFFFFFFFF);
    paint.AddTexturedQuad({70, 10, 80, 20}, {0, 0, 1, 1}, 8, 0xFFFFFFFF);
    paint.AddTexturedQuad({70, 30, 80, 40}, {0, 0, 1, 1}, 7, 0xFFFFFFFF);
    paint.AddLine(90, 30, 100, 30, 2.f, 0xFFFFFFFF);
    paint.PushClip({5, 5, 150, 150});
    paint.AddLine(90, 40, 100, 40, 2.f, 0xFFFFFFFF);
    paint.PopClip();
    paint.AddRoundedRect({90, 50, 100, 60}, 0xFFFFFFFF, 2.f);

    const auto                      &data = paint.GetDrawData();
    const std::array<uint32_t, 8>    counts{12, 9, 12, 6, 6, 6, 6, 6};
    const std::array<UITextureId, 8> textures{0, 0, 7, 8, 7, 0, 0, 0};
    const std::array<bool, 8>        shapes{true, false, false, false, false, true, true, true};
    ASSERT_EQ(data.commands.size(), counts.size());
    uint32_t offset = 0;
    for (size_t i = 0; i < counts.size(); ++i) {
        const auto &command = data.commands[i];
        EXPECT_EQ(command.indexOffset, offset);
        EXPECT_EQ(command.indexCount, counts[i]);
        EXPECT_EQ(command.textureId, textures[i]);
        EXPECT_EQ(command.shape, shapes[i]);
        EXPECT_FLOAT_EQ(command.clip.left, i == 6 ? 5.f : 0.f);
        EXPECT_FLOAT_EQ(command.clip.top, i == 6 ? 5.f : 0.f);
        EXPECT_FLOAT_EQ(command.clip.right, i == 6 ? 150.f : 200.f);
        EXPECT_FLOAT_EQ(command.clip.bottom, i == 6 ? 150.f : 200.f);
        offset += command.indexCount;
    }
    EXPECT_EQ(offset, data.indices.size());
    for (const uint32_t index : data.indices)
        EXPECT_LT(index, data.vertices.size());
}
