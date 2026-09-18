//
// GeometryGenerator tests: primitive stream validity.
//

#include <gtest/gtest.h>

#include <core/math/GeometryGenerator.h>

#include <cmath>

using namespace sky;

namespace {

    void CheckStream(const GeometryStreams &s)
    {
        ASSERT_FALSE(s.positions.empty());
        const size_t vertexCount = s.positions.size();
        ASSERT_EQ(s.normals.size(), vertexCount);
        ASSERT_EQ(s.tangents.size(), vertexCount);
        ASSERT_EQ(s.uvs.size(), vertexCount);
        ASSERT_FALSE(s.indices.empty());
        EXPECT_EQ(s.indices.size() % 3u, 0u);

        for (size_t i = 0; i < vertexCount; ++i) {
            EXPECT_NEAR(s.normals[i].Length(), 1.0f, 1e-4f);

            const Vector4 &tg = s.tangents[i];
            const Vector3 t(tg.x, tg.y, tg.z);
            EXPECT_NEAR(t.Length(), 1.0f, 1e-4f);
            EXPECT_NEAR(t.Dot(s.normals[i]), 0.0f, 1e-3f);
            EXPECT_NEAR(std::abs(tg.w), 1.0f, 1e-4f);

            EXPECT_GE(s.uvs[i].x, -1e-5f);
            EXPECT_LE(s.uvs[i].x, 1.0f + 1e-5f);
            EXPECT_GE(s.uvs[i].y, -1e-5f);
            EXPECT_LE(s.uvs[i].y, 1.0f + 1e-5f);
        }

        for (uint32_t idx : s.indices) {
            EXPECT_LT(idx, vertexCount);
        }
    }

} // namespace

TEST(GeometryGeneratorTest, Cube)
{
    auto s = GenerateCube(1.0f);
    CheckStream(s);
    EXPECT_EQ(s.positions.size(), 24u);
    EXPECT_EQ(s.indices.size(), 36u);
}

TEST(GeometryGeneratorTest, Plane)
{
    auto s = GeneratePlane(2.0f, 2.0f, 2, 2);
    CheckStream(s);
    EXPECT_EQ(s.positions.size(), 9u);
    EXPECT_EQ(s.indices.size(), 24u);
}

TEST(GeometryGeneratorTest, Sphere)
{
    auto s = GenerateSphere(1.0f, 16, 32);
    CheckStream(s);
    EXPECT_EQ(s.positions.size(), (16u + 1u) * (32u + 1u));
}

TEST(GeometryGeneratorTest, Cylinder)
{
    auto s = GenerateCylinder(0.5f, 0.5f, 2.0f, 32);
    CheckStream(s);
}

TEST(GeometryGeneratorTest, Cone)
{
    auto s = GenerateCone(0.5f, 2.0f, 32);
    CheckStream(s);
}

TEST(GeometryGeneratorTest, Capsule)
{
    auto s = GenerateCapsule(0.5f, 2.0f, 16, 32);
    CheckStream(s);
}
