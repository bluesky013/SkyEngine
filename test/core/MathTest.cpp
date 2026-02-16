//
// Created by blues on 2024/11/25.
//

#include <gtest/gtest.h>
#include <core/math/MathUtil.h>

using namespace sky;

TEST(MathTest, MathUtilTest)
{
    for (uint32_t i = 0; i < 32; ++i) {
        ASSERT_EQ(CeilLog2(static_cast<uint32_t>(pow(2, i))), i);
    }
}

TEST(MathTest, Vector4Add)
{
    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 c = a + b;
    ASSERT_FLOAT_EQ(c.x, 6.0f);
    ASSERT_FLOAT_EQ(c.y, 8.0f);
    ASSERT_FLOAT_EQ(c.z, 10.0f);
    ASSERT_FLOAT_EQ(c.w, 12.0f);
}

TEST(MathTest, Vector4Sub)
{
    Vector4 a(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 b(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 c = a - b;
    ASSERT_FLOAT_EQ(c.x, 4.0f);
    ASSERT_FLOAT_EQ(c.y, 4.0f);
    ASSERT_FLOAT_EQ(c.z, 4.0f);
    ASSERT_FLOAT_EQ(c.w, 4.0f);
}

TEST(MathTest, Vector4Mul)
{
    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 c = a * b;
    ASSERT_FLOAT_EQ(c.x, 5.0f);
    ASSERT_FLOAT_EQ(c.y, 12.0f);
    ASSERT_FLOAT_EQ(c.z, 21.0f);
    ASSERT_FLOAT_EQ(c.w, 32.0f);
}

TEST(MathTest, Vector4Div)
{
    Vector4 a(10.0f, 12.0f, 21.0f, 32.0f);
    Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 c = a / b;
    ASSERT_FLOAT_EQ(c.x, 2.0f);
    ASSERT_FLOAT_EQ(c.y, 2.0f);
    ASSERT_FLOAT_EQ(c.z, 3.0f);
    ASSERT_FLOAT_EQ(c.w, 4.0f);
}

TEST(MathTest, Vector4ScalarMul)
{
    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 c = a * 3.0f;
    ASSERT_FLOAT_EQ(c.x, 3.0f);
    ASSERT_FLOAT_EQ(c.y, 6.0f);
    ASSERT_FLOAT_EQ(c.z, 9.0f);
    ASSERT_FLOAT_EQ(c.w, 12.0f);
}

TEST(MathTest, Vector4ScalarDiv)
{
    Vector4 a(2.0f, 4.0f, 6.0f, 8.0f);
    Vector4 c = a / 2.0f;
    ASSERT_FLOAT_EQ(c.x, 1.0f);
    ASSERT_FLOAT_EQ(c.y, 2.0f);
    ASSERT_FLOAT_EQ(c.z, 3.0f);
    ASSERT_FLOAT_EQ(c.w, 4.0f);
}

TEST(MathTest, Vector4Dot)
{
    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);
    float dot = a.Dot(b);
    ASSERT_FLOAT_EQ(dot, 70.0f);
}

TEST(MathTest, Vector4Negate)
{
    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 c = -a;
    ASSERT_FLOAT_EQ(c.x, -1.0f);
    ASSERT_FLOAT_EQ(c.y, -2.0f);
    ASSERT_FLOAT_EQ(c.z, -3.0f);
    ASSERT_FLOAT_EQ(c.w, -4.0f);
}

TEST(MathTest, Matrix4VecMul)
{
    Matrix4 id = Matrix4::Identity();
    Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 r = id * v;
    ASSERT_FLOAT_EQ(r.x, 1.0f);
    ASSERT_FLOAT_EQ(r.y, 2.0f);
    ASSERT_FLOAT_EQ(r.z, 3.0f);
    ASSERT_FLOAT_EQ(r.w, 4.0f);
}

TEST(MathTest, Matrix4MatMul)
{
    Matrix4 id = Matrix4::Identity();
    Matrix4 r = id * id;
    ASSERT_FLOAT_EQ(r[0][0], 1.0f);
    ASSERT_FLOAT_EQ(r[1][1], 1.0f);
    ASSERT_FLOAT_EQ(r[2][2], 1.0f);
    ASSERT_FLOAT_EQ(r[3][3], 1.0f);
    ASSERT_FLOAT_EQ(r[0][1], 0.0f);
    ASSERT_FLOAT_EQ(r[1][0], 0.0f);
}