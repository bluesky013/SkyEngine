//
// Created by blues on 2024/11/25.
//

#include <gtest/gtest.h>
#include <core/math/MathUtil.h>
#include <core/math/Vector4.h>
#include <core/math/Matrix4.h>

using namespace sky;

TEST(MathTest, MathUtilTest)
{
    for (uint32_t i = 0; i < 32; ++i) {
        ASSERT_EQ(CeilLog2(static_cast<uint32_t>(pow(2, i))), i);
    }
}

// Vector4 Tests
TEST(MathTest, Vector4Constructors)
{
    // Test default constructor
    Vector4 v0;
    ASSERT_EQ(v0.x, 0.0f);
    ASSERT_EQ(v0.y, 0.0f);
    ASSERT_EQ(v0.z, 0.0f);
    ASSERT_EQ(v0.w, 0.0f);

    // Test scalar constructor
    Vector4 v1(5.0f);
    ASSERT_EQ(v1.x, 5.0f);
    ASSERT_EQ(v1.y, 5.0f);
    ASSERT_EQ(v1.z, 5.0f);
    ASSERT_EQ(v1.w, 5.0f);

    // Test component constructor
    Vector4 v2(1.0f, 2.0f, 3.0f, 4.0f);
    ASSERT_EQ(v2.x, 1.0f);
    ASSERT_EQ(v2.y, 2.0f);
    ASSERT_EQ(v2.z, 3.0f);
    ASSERT_EQ(v2.w, 4.0f);
}

TEST(MathTest, Vector4Arithmetic)
{
    Vector4 v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 v2(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4 v3(2.0f, 2.0f, 2.0f, 2.0f);

    // Test addition
    Vector4 sum = v1 + v2;
    ASSERT_EQ(sum.x, 2.0f);
    ASSERT_EQ(sum.y, 3.0f);
    ASSERT_EQ(sum.z, 4.0f);
    ASSERT_EQ(sum.w, 5.0f);

    // Test subtraction
    Vector4 diff = v1 - v2;
    ASSERT_EQ(diff.x, 0.0f);
    ASSERT_EQ(diff.y, 1.0f);
    ASSERT_EQ(diff.z, 2.0f);
    ASSERT_EQ(diff.w, 3.0f);

    // Test negation
    Vector4 neg = -v1;
    ASSERT_EQ(neg.x, -1.0f);
    ASSERT_EQ(neg.y, -2.0f);
    ASSERT_EQ(neg.z, -3.0f);
    ASSERT_EQ(neg.w, -4.0f);

    // Test scalar multiplication
    Vector4 scaled = v1 * 2.0f;
    ASSERT_EQ(scaled.x, 2.0f);
    ASSERT_EQ(scaled.y, 4.0f);
    ASSERT_EQ(scaled.z, 6.0f);
    ASSERT_EQ(scaled.w, 8.0f);

    // Test element-wise multiplication
    Vector4 elemMul = v1 * v2;
    ASSERT_EQ(elemMul.x, 1.0f);
    ASSERT_EQ(elemMul.y, 2.0f);
    ASSERT_EQ(elemMul.z, 3.0f);
    ASSERT_EQ(elemMul.w, 4.0f);

    // Test scalar division
    Vector4 divided = v3 / 2.0f;
    ASSERT_EQ(divided.x, 1.0f);
    ASSERT_EQ(divided.y, 1.0f);
    ASSERT_EQ(divided.z, 1.0f);
    ASSERT_EQ(divided.w, 1.0f);

    // Test element-wise division
    Vector4 elemDiv = v3 / v2;
    ASSERT_EQ(elemDiv.x, 2.0f);
    ASSERT_EQ(elemDiv.y, 2.0f);
    ASSERT_EQ(elemDiv.z, 2.0f);
    ASSERT_EQ(elemDiv.w, 2.0f);
}

TEST(MathTest, Vector4Operators)
{
    Vector4 v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 v2(2.0f, 2.0f, 2.0f, 2.0f);

    // Test subscript operator
    ASSERT_EQ(v1[0], 1.0f);
    ASSERT_EQ(v1[1], 2.0f);
    ASSERT_EQ(v1[2], 3.0f);
    ASSERT_EQ(v1[3], 4.0f);

    // Test compound operators
    Vector4 v3 = v1;
    v3 += v2;
    ASSERT_EQ(v3.x, 3.0f);
    ASSERT_EQ(v3.y, 4.0f);
    ASSERT_EQ(v3.z, 5.0f);
    ASSERT_EQ(v3.w, 6.0f);

    Vector4 v4 = v1;
    v4 -= Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ASSERT_EQ(v4.x, 0.0f);
    ASSERT_EQ(v4.y, 1.0f);

    Vector4 v5 = v1;
    v5 *= 2.0f;
    ASSERT_EQ(v5.x, 2.0f);
    ASSERT_EQ(v5.y, 4.0f);

    Vector4 v6(4.0f, 4.0f, 4.0f, 4.0f);
    v6 /= 2.0f;
    ASSERT_EQ(v6.x, 2.0f);
    ASSERT_EQ(v6.y, 2.0f);
}

TEST(MathTest, Vector4DotProduct)
{
    Vector4 v1(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 v2(2.0f, 3.0f, 4.0f, 5.0f);

    // Test dot product: 1*2 + 2*3 + 3*4 + 4*5 = 2 + 6 + 12 + 20 = 40
    float dot = v1.Dot(v2);
    ASSERT_EQ(dot, 40.0f);

    // Test dot product with itself
    float dotSelf = v1.Dot(v1);
    ASSERT_EQ(dotSelf, 30.0f); // 1 + 4 + 9 + 16 = 30
}

TEST(MathTest, Vector4Normalization)
{
    Vector4 v(3.0f, 4.0f, 0.0f, 0.0f);
    float origLength = sqrt(v.Dot(v));
    ASSERT_FLOAT_EQ(origLength, 5.0f);

    // Test normalize
    v.Normalize();
    float normLength = sqrt(v.Dot(v));
    ASSERT_FLOAT_EQ(normLength, 1.0f);

    // Check normalized values
    ASSERT_FLOAT_EQ(v.x, 0.6f);
    ASSERT_FLOAT_EQ(v.y, 0.8f);
    ASSERT_FLOAT_EQ(v.z, 0.0f);
    ASSERT_FLOAT_EQ(v.w, 0.0f);
}

// Matrix4 Tests
TEST(MathTest, Matrix4Identity)
{
    Matrix4 identity = Matrix4::Identity();

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            ASSERT_EQ(identity.m[i][j], expected);
        }
    }
}

TEST(MathTest, Matrix4Constructors)
{
    // Test default constructor
    Matrix4 m0;

    // Test row vector constructor
    Vector4 r0(1.0f, 0.0f, 0.0f, 0.0f);
    Vector4 r1(0.0f, 2.0f, 0.0f, 0.0f);
    Vector4 r2(0.0f, 0.0f, 3.0f, 0.0f);
    Vector4 r3(0.0f, 0.0f, 0.0f, 4.0f);

    Matrix4 m1(r0, r1, r2, r3);
    ASSERT_EQ(m1.m[0][0], 1.0f);
    ASSERT_EQ(m1.m[1][1], 2.0f);
    ASSERT_EQ(m1.m[2][2], 3.0f);
    ASSERT_EQ(m1.m[3][3], 4.0f);
}

TEST(MathTest, Matrix4Arithmetic)
{
    Vector4 r0(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 r1(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 r2(9.0f, 10.0f, 11.0f, 12.0f);
    Vector4 r3(13.0f, 14.0f, 15.0f, 16.0f);

    Matrix4 m1(r0, r1, r2, r3);
    Matrix4 m2 = m1;

    // Test addition
    Matrix4 sum = m1 + m2;
    ASSERT_EQ(sum.m[0][0], 2.0f);
    ASSERT_EQ(sum.m[1][1], 12.0f);
    ASSERT_EQ(sum.m[3][3], 32.0f);

    // Test subtraction
    Matrix4 diff = m1 - m2;
    ASSERT_EQ(diff.m[0][0], 0.0f);
    ASSERT_EQ(diff.m[1][1], 0.0f);
    ASSERT_EQ(diff.m[2][2], 0.0f);

    // Test negation
    Matrix4 neg = -m1;
    ASSERT_EQ(neg.m[0][0], -1.0f);
    ASSERT_EQ(neg.m[1][1], -6.0f);

    // Test scalar multiplication
    Matrix4 scaled = m1 * 2.0f;
    ASSERT_EQ(scaled.m[0][0], 2.0f);
    ASSERT_EQ(scaled.m[1][1], 12.0f);

    // Test scalar division
    Matrix4 divided = scaled / 2.0f;
    ASSERT_EQ(divided.m[0][0], 1.0f);
    ASSERT_EQ(divided.m[1][1], 6.0f);
}

TEST(MathTest, Matrix4Multiplication)
{
    // Test matrix * identity = matrix
    Matrix4 identity = Matrix4::Identity();
    Vector4 r0(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 r1(5.0f, 6.0f, 7.0f, 8.0f);
    Vector4 r2(9.0f, 10.0f, 11.0f, 12.0f);
    Vector4 r3(13.0f, 14.0f, 15.0f, 16.0f);

    Matrix4 m(r0, r1, r2, r3);
    Matrix4 result = m * identity;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            ASSERT_FLOAT_EQ(result.m[i][j], m.m[i][j]);
        }
    }
}

TEST(MathTest, Matrix4VectorMultiplication)
{
    Matrix4 identity = Matrix4::Identity();
    Vector4 v(1.0f, 2.0f, 3.0f, 4.0f);

    Vector4 result = identity * v;
    ASSERT_EQ(result.x, 1.0f);
    ASSERT_EQ(result.y, 2.0f);
    ASSERT_EQ(result.z, 3.0f);
    ASSERT_EQ(result.w, 4.0f);

    // Test with scaling matrix
    Vector4 r0(2.0f, 0.0f, 0.0f, 0.0f);
    Vector4 r1(0.0f, 3.0f, 0.0f, 0.0f);
    Vector4 r2(0.0f, 0.0f, 4.0f, 0.0f);
    Vector4 r3(0.0f, 0.0f, 0.0f, 1.0f);

    Matrix4 scale(r0, r1, r2, r3);
    Vector4 scaled = scale * v;

    ASSERT_EQ(scaled.x, 2.0f);
    ASSERT_EQ(scaled.y, 6.0f);
    ASSERT_EQ(scaled.z, 12.0f);
    ASSERT_EQ(scaled.w, 4.0f);
}

TEST(MathTest, Matrix4Operators)
{
    Vector4 r0(1.0f, 0.0f, 0.0f, 0.0f);
    Vector4 r1(0.0f, 2.0f, 0.0f, 0.0f);
    Vector4 r2(0.0f, 0.0f, 3.0f, 0.0f);
    Vector4 r3(0.0f, 0.0f, 0.0f, 4.0f);

    Matrix4 m(r0, r1, r2, r3);

    // Test row access operator
    ASSERT_EQ(m[0][0], 1.0f);
    ASSERT_EQ(m[1][1], 2.0f);
    ASSERT_EQ(m[2][2], 3.0f);
    ASSERT_EQ(m[3][3], 4.0f);

    // Test compound operators
    Matrix4 m2 = m;
    m2 += Matrix4::Identity();
    ASSERT_EQ(m2.m[0][0], 2.0f);
    ASSERT_EQ(m2.m[1][1], 3.0f);

    Matrix4 m3 = m;
    m3 -= Matrix4::Identity();
    ASSERT_EQ(m3.m[0][0], 0.0f);
    ASSERT_EQ(m3.m[1][1], 1.0f);

    Matrix4 m4 = m;
    m4 *= 2.0f;
    ASSERT_EQ(m4.m[0][0], 2.0f);
    ASSERT_EQ(m4.m[3][3], 8.0f);

    Matrix4 m5(r0, r1, r2, r3);
    m5 /= 2.0f;
    ASSERT_EQ(m5.m[0][0], 0.5f);
    ASSERT_EQ(m5.m[3][3], 2.0f);
}

TEST(MathTest, Matrix4Translate)
{
    Matrix4 m = Matrix4::Identity();
    Vector3 translation(1.0f, 2.0f, 3.0f);

    m.Translate(translation);

    // After translation, the last row should contain the translation
    ASSERT_EQ(m.m[3][0], 1.0f);
    ASSERT_EQ(m.m[3][1], 2.0f);
    ASSERT_EQ(m.m[3][2], 3.0f);
}

TEST(MathTest, Matrix4Determinant)
{
    // Test identity matrix determinant (should be 1)
    Matrix4 identity = Matrix4::Identity();
    float det = identity.Determinant();
    ASSERT_FLOAT_EQ(det, 1.0f);
}

TEST(MathTest, Matrix4Inverse)
{
    // Test identity matrix inverse (should be identity)
    Matrix4 identity = Matrix4::Identity();
    Matrix4 invIdentity = identity.Inverse();

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            ASSERT_FLOAT_EQ(invIdentity.m[i][j], expected);
        }
    }
}
