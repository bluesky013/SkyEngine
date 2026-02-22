//
// Created by blues on 2026/2/19.
//

#include <core/shapes/Bounds.h>
#include <core/math/MathUtil.h>
#include <gtest/gtest.h>
#include <cmath>

using namespace sky;

static constexpr float EPSILON = 1e-5f;

TEST(BoundsTest, DefaultConstructorTest)
{
    BoundingBoxSphere box;
    ASSERT_FLOAT_EQ(box.center.x, 0.f);
    ASSERT_FLOAT_EQ(box.center.y, 0.f);
    ASSERT_FLOAT_EQ(box.center.z, 0.f);
    ASSERT_FLOAT_EQ(box.extent.x, 0.f);
    ASSERT_FLOAT_EQ(box.extent.y, 0.f);
    ASSERT_FLOAT_EQ(box.extent.z, 0.f);
    ASSERT_FLOAT_EQ(box.radius, 0.f);
}

TEST(BoundsTest, CenterExtentRadiusConstructorTest)
{
    BoundingBoxSphere box(Vector3(1, 2, 3), Vector3(4, 5, 6), 10.f);
    ASSERT_FLOAT_EQ(box.center.x, 1.f);
    ASSERT_FLOAT_EQ(box.center.y, 2.f);
    ASSERT_FLOAT_EQ(box.center.z, 3.f);
    ASSERT_FLOAT_EQ(box.extent.x, 4.f);
    ASSERT_FLOAT_EQ(box.extent.y, 5.f);
    ASSERT_FLOAT_EQ(box.extent.z, 6.f);
    ASSERT_FLOAT_EQ(box.radius, 10.f);
}

TEST(BoundsTest, CenterExtentConstructorTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(3, 4, 0));
    ASSERT_FLOAT_EQ(box.center.x, 0.f);
    ASSERT_FLOAT_EQ(box.center.y, 0.f);
    ASSERT_FLOAT_EQ(box.center.z, 0.f);
    ASSERT_FLOAT_EQ(box.extent.x, 3.f);
    ASSERT_FLOAT_EQ(box.extent.y, 4.f);
    ASSERT_FLOAT_EQ(box.extent.z, 0.f);
    ASSERT_FLOAT_EQ(box.radius, 5.f); // sqrt(9 + 16 + 0) = 5
}

TEST(BoundsTest, AABBConstructorTest)
{
    AABB aabb(Vector3(-2, -3, -4), Vector3(2, 3, 4));
    BoundingBoxSphere box(aabb);

    ASSERT_FLOAT_EQ(box.center.x, 0.f);
    ASSERT_FLOAT_EQ(box.center.y, 0.f);
    ASSERT_FLOAT_EQ(box.center.z, 0.f);
    ASSERT_FLOAT_EQ(box.extent.x, 2.f);
    ASSERT_FLOAT_EQ(box.extent.y, 3.f);
    ASSERT_FLOAT_EQ(box.extent.z, 4.f);
    ASSERT_NEAR(box.radius, std::sqrt(4.f + 9.f + 16.f), EPSILON);
}

TEST(BoundsTest, AABBConstructorOffCenterTest)
{
    AABB aabb(Vector3(1, 2, 3), Vector3(5, 8, 9));
    BoundingBoxSphere box(aabb);

    ASSERT_FLOAT_EQ(box.center.x, 3.f);
    ASSERT_FLOAT_EQ(box.center.y, 5.f);
    ASSERT_FLOAT_EQ(box.center.z, 6.f);
    ASSERT_FLOAT_EQ(box.extent.x, 2.f);
    ASSERT_FLOAT_EQ(box.extent.y, 3.f);
    ASSERT_FLOAT_EQ(box.extent.z, 3.f);
}

TEST(BoundsTest, FromMinMaxTest)
{
    auto box = BoundingBoxSphere::FromMinMax(Vector3(-1, -2, -3), Vector3(1, 2, 3));
    ASSERT_FLOAT_EQ(box.center.x, 0.f);
    ASSERT_FLOAT_EQ(box.center.y, 0.f);
    ASSERT_FLOAT_EQ(box.center.z, 0.f);
    ASSERT_FLOAT_EQ(box.extent.x, 1.f);
    ASSERT_FLOAT_EQ(box.extent.y, 2.f);
    ASSERT_FLOAT_EQ(box.extent.z, 3.f);
}

TEST(BoundsTest, ToAABBTest)
{
    BoundingBoxSphere box(Vector3(1, 2, 3), Vector3(4, 5, 6));
    AABB aabb = box.ToAABB();

    ASSERT_FLOAT_EQ(aabb.min.x, -3.f);
    ASSERT_FLOAT_EQ(aabb.min.y, -3.f);
    ASSERT_FLOAT_EQ(aabb.min.z, -3.f);
    ASSERT_FLOAT_EQ(aabb.max.x, 5.f);
    ASSERT_FLOAT_EQ(aabb.max.y, 7.f);
    ASSERT_FLOAT_EQ(aabb.max.z, 9.f);
}

TEST(BoundsTest, MinMaxTest)
{
    BoundingBoxSphere box(Vector3(1, 2, 3), Vector3(4, 5, 6));
    Vector3 minPt = box.Min();
    Vector3 maxPt = box.Max();

    ASSERT_FLOAT_EQ(minPt.x, -3.f);
    ASSERT_FLOAT_EQ(minPt.y, -3.f);
    ASSERT_FLOAT_EQ(minPt.z, -3.f);
    ASSERT_FLOAT_EQ(maxPt.x, 5.f);
    ASSERT_FLOAT_EQ(maxPt.y, 7.f);
    ASSERT_FLOAT_EQ(maxPt.z, 9.f);
}

TEST(BoundsTest, AABBRoundTripTest)
{
    AABB original(Vector3(-3, -5, -7), Vector3(3, 5, 7));
    BoundingBoxSphere box(original);
    AABB result = box.ToAABB();

    ASSERT_NEAR(result.min.x, original.min.x, EPSILON);
    ASSERT_NEAR(result.min.y, original.min.y, EPSILON);
    ASSERT_NEAR(result.min.z, original.min.z, EPSILON);
    ASSERT_NEAR(result.max.x, original.max.x, EPSILON);
    ASSERT_NEAR(result.max.y, original.max.y, EPSILON);
    ASSERT_NEAR(result.max.z, original.max.z, EPSILON);
}

TEST(BoundsTest, ContainsPointTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    // Center point
    ASSERT_TRUE(box.Contains(Vector3(0, 0, 0)));

    // Points on boundary
    ASSERT_TRUE(box.Contains(Vector3(1, 0, 0)));
    ASSERT_TRUE(box.Contains(Vector3(0, 1, 0)));
    ASSERT_TRUE(box.Contains(Vector3(0, 0, 1)));
    ASSERT_TRUE(box.Contains(Vector3(-1, -1, -1)));

    // Points outside
    ASSERT_FALSE(box.Contains(Vector3(1.1f, 0, 0)));
    ASSERT_FALSE(box.Contains(Vector3(0, 1.1f, 0)));
    ASSERT_FALSE(box.Contains(Vector3(0, 0, 1.1f)));
    ASSERT_FALSE(box.Contains(Vector3(2, 2, 2)));
}

TEST(BoundsTest, ContainsPointOffCenterTest)
{
    BoundingBoxSphere box(Vector3(5, 5, 5), Vector3(2, 2, 2));

    ASSERT_TRUE(box.Contains(Vector3(5, 5, 5)));
    ASSERT_TRUE(box.Contains(Vector3(3, 3, 3)));
    ASSERT_TRUE(box.Contains(Vector3(7, 7, 7)));
    ASSERT_FALSE(box.Contains(Vector3(2.9f, 5, 5)));
    ASSERT_FALSE(box.Contains(Vector3(7.1f, 5, 5)));
}

TEST(BoundsTest, IntersectsOverlappingTest)
{
    BoundingBoxSphere a(Vector3(0, 0, 0), Vector3(1, 1, 1));
    BoundingBoxSphere b(Vector3(1.5f, 0, 0), Vector3(1, 1, 1));

    // Overlapping
    ASSERT_TRUE(a.Intersects(b));
    ASSERT_TRUE(b.Intersects(a));
}

TEST(BoundsTest, IntersectsTouchingTest)
{
    BoundingBoxSphere a(Vector3(0, 0, 0), Vector3(1, 1, 1));
    BoundingBoxSphere c(Vector3(2, 0, 0), Vector3(1, 1, 1));

    // Touching at boundary
    ASSERT_TRUE(a.Intersects(c));
    ASSERT_TRUE(c.Intersects(a));
}

TEST(BoundsTest, IntersectsSeparatedTest)
{
    BoundingBoxSphere a(Vector3(0, 0, 0), Vector3(1, 1, 1));
    BoundingBoxSphere d(Vector3(2.1f, 0, 0), Vector3(1, 1, 1));

    // Separated
    ASSERT_FALSE(a.Intersects(d));
    ASSERT_FALSE(d.Intersects(a));
}

TEST(BoundsTest, IntersectsSelfTest)
{
    BoundingBoxSphere a(Vector3(3, 4, 5), Vector3(1, 2, 3));
    ASSERT_TRUE(a.Intersects(a));
}

TEST(BoundsTest, MergeTest)
{
    BoundingBoxSphere a(Vector3(0, 0, 0), Vector3(1, 1, 1));
    BoundingBoxSphere b(Vector3(4, 0, 0), Vector3(1, 1, 1));

    auto merged = BoundingBoxSphere::Merge(a, b);

    // a: min=(-1,-1,-1) max=(1,1,1)
    // b: min=(3,-1,-1) max=(5,1,1)
    // merged: min=(-1,-1,-1) max=(5,1,1) => center=(2,0,0) extent=(3,1,1)
    ASSERT_FLOAT_EQ(merged.center.x, 2.f);
    ASSERT_FLOAT_EQ(merged.center.y, 0.f);
    ASSERT_FLOAT_EQ(merged.center.z, 0.f);
    ASSERT_FLOAT_EQ(merged.extent.x, 3.f);
    ASSERT_FLOAT_EQ(merged.extent.y, 1.f);
    ASSERT_FLOAT_EQ(merged.extent.z, 1.f);
}

TEST(BoundsTest, MergeContainedTest)
{
    BoundingBoxSphere outer(Vector3(0, 0, 0), Vector3(5, 5, 5));
    BoundingBoxSphere inner(Vector3(0, 0, 0), Vector3(1, 1, 1));

    auto merged = BoundingBoxSphere::Merge(outer, inner);
    ASSERT_FLOAT_EQ(merged.center.x, 0.f);
    ASSERT_FLOAT_EQ(merged.center.y, 0.f);
    ASSERT_FLOAT_EQ(merged.center.z, 0.f);
    ASSERT_FLOAT_EQ(merged.extent.x, 5.f);
    ASSERT_FLOAT_EQ(merged.extent.y, 5.f);
    ASSERT_FLOAT_EQ(merged.extent.z, 5.f);
}

TEST(BoundsTest, ExpandedTest)
{
    BoundingBoxSphere box(Vector3(1, 2, 3), Vector3(1, 1, 1));
    auto expanded = box.Expanded(2.f);

    ASSERT_FLOAT_EQ(expanded.center.x, 1.f);
    ASSERT_FLOAT_EQ(expanded.center.y, 2.f);
    ASSERT_FLOAT_EQ(expanded.center.z, 3.f);
    ASSERT_FLOAT_EQ(expanded.extent.x, 3.f);
    ASSERT_FLOAT_EQ(expanded.extent.y, 3.f);
    ASSERT_FLOAT_EQ(expanded.extent.z, 3.f);
}

TEST(BoundsTest, DistanceToInsideTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(2, 2, 2));

    // Point inside -> distance is 0
    ASSERT_FLOAT_EQ(box.DistanceTo(Vector3(0, 0, 0)), 0.f);
    ASSERT_FLOAT_EQ(box.DistanceTo(Vector3(1, 1, 1)), 0.f);
}

TEST(BoundsTest, DistanceToOutsideAxisAlignedTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    // Distance along single axis
    ASSERT_NEAR(box.DistanceTo(Vector3(4, 0, 0)), 3.f, EPSILON);
    ASSERT_NEAR(box.DistanceTo(Vector3(0, -4, 0)), 3.f, EPSILON);
    ASSERT_NEAR(box.DistanceTo(Vector3(0, 0, 6)), 5.f, EPSILON);
}

TEST(BoundsTest, DistanceToOutsideCornerTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    // Distance to corner: point at (4,4,1) -> dx=3, dy=3, dz=0 -> sqrt(18)
    ASSERT_NEAR(box.DistanceTo(Vector3(4, 4, 1)), std::sqrt(18.f), EPSILON);
}

TEST(BoundsTest, RadiusAutoComputeTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 0, 0));
    ASSERT_FLOAT_EQ(box.radius, 1.f);

    BoundingBoxSphere box2(Vector3(0, 0, 0), Vector3(1, 1, 1));
    ASSERT_NEAR(box2.radius, std::sqrt(3.f), EPSILON);
}

TEST(BoundsTest, TransformIdentityTest)
{
    BoundingBoxSphere box(Vector3(1, 2, 3), Vector3(4, 5, 6));
    auto result = BoundingBoxSphere::Transform(box, Matrix4::Identity());

    ASSERT_NEAR(result.center.x, 1.f, EPSILON);
    ASSERT_NEAR(result.center.y, 2.f, EPSILON);
    ASSERT_NEAR(result.center.z, 3.f, EPSILON);
    ASSERT_NEAR(result.extent.x, 4.f, EPSILON);
    ASSERT_NEAR(result.extent.y, 5.f, EPSILON);
    ASSERT_NEAR(result.extent.z, 6.f, EPSILON);
}

TEST(BoundsTest, TransformTranslationTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    // Build a pure translation matrix: translate by (10, 20, 30)
    Matrix4 mat = Matrix4::Identity();
    mat[3][0] = 10.f;
    mat[3][1] = 20.f;
    mat[3][2] = 30.f;

    auto result = BoundingBoxSphere::Transform(box, mat);

    // Center should be translated
    ASSERT_NEAR(result.center.x, 10.f, EPSILON);
    ASSERT_NEAR(result.center.y, 20.f, EPSILON);
    ASSERT_NEAR(result.center.z, 30.f, EPSILON);

    // Extent should remain the same (no rotation/scale)
    ASSERT_NEAR(result.extent.x, 1.f, EPSILON);
    ASSERT_NEAR(result.extent.y, 1.f, EPSILON);
    ASSERT_NEAR(result.extent.z, 1.f, EPSILON);
}

TEST(BoundsTest, TransformUniformScaleTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    // Build a uniform scale matrix: scale by 3
    Matrix4 mat = Matrix4::Identity();
    mat[0][0] = 3.f;
    mat[1][1] = 3.f;
    mat[2][2] = 3.f;

    auto result = BoundingBoxSphere::Transform(box, mat);

    ASSERT_NEAR(result.center.x, 0.f, EPSILON);
    ASSERT_NEAR(result.center.y, 0.f, EPSILON);
    ASSERT_NEAR(result.center.z, 0.f, EPSILON);

    ASSERT_NEAR(result.extent.x, 3.f, EPSILON);
    ASSERT_NEAR(result.extent.y, 3.f, EPSILON);
    ASSERT_NEAR(result.extent.z, 3.f, EPSILON);

    // radius = min(originalRadius * maxScale, newExtent.Length())
    // originalRadius = sqrt(3), maxScale = 3, so originalRadius * maxScale = 3*sqrt(3) ≈ 5.196
    // newExtent.Length() = sqrt(9+9+9) = 3*sqrt(3) ≈ 5.196
    // They should be equal for uniform scale
    ASSERT_NEAR(result.radius, std::sqrt(3.f) * 3.f, EPSILON);
}

TEST(BoundsTest, TransformNonUniformScaleTest)
{
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 1, 1));

    // Build a non-uniform scale matrix: scale (2, 1, 1)
    Matrix4 mat = Matrix4::Identity();
    mat[0][0] = 2.f;
    mat[1][1] = 1.f;
    mat[2][2] = 1.f;

    auto result = BoundingBoxSphere::Transform(box, mat);

    ASSERT_NEAR(result.extent.x, 2.f, EPSILON);
    ASSERT_NEAR(result.extent.y, 1.f, EPSILON);
    ASSERT_NEAR(result.extent.z, 1.f, EPSILON);

    // originalRadius = sqrt(3), maxScale = 2, so originalRadius * maxScale = 2*sqrt(3) ≈ 3.464
    // newExtent.Length() = sqrt(4+1+1) = sqrt(6) ≈ 2.449
    // radius = min(2*sqrt(3), sqrt(6)) = sqrt(6)
    ASSERT_NEAR(result.radius, std::sqrt(6.f), EPSILON);
}

TEST(BoundsTest, TransformTranslationAndScaleTest)
{
    BoundingBoxSphere box(Vector3(1, 2, 3), Vector3(1, 1, 1));

    // Scale by 2, then translate by (10, 0, 0)
    Matrix4 mat = Matrix4::Identity();
    mat[0][0] = 2.f;
    mat[1][1] = 2.f;
    mat[2][2] = 2.f;
    mat[3][0] = 10.f;

    auto result = BoundingBoxSphere::Transform(box, mat);

    // Original AABB: min=(0,1,2) max=(2,3,4)
    // After scale by 2: min=(0,2,4) max=(4,6,8)
    // After translate by (10,0,0): min=(10,2,4) max=(14,6,8)
    // center = (12, 4, 6), extent = (2, 2, 2)
    ASSERT_NEAR(result.center.x, 12.f, EPSILON);
    ASSERT_NEAR(result.center.y, 4.f, EPSILON);
    ASSERT_NEAR(result.center.z, 6.f, EPSILON);
    ASSERT_NEAR(result.extent.x, 2.f, EPSILON);
    ASSERT_NEAR(result.extent.y, 2.f, EPSILON);
    ASSERT_NEAR(result.extent.z, 2.f, EPSILON);
}

TEST(BoundsTest, TransformPreservesRadiusBound)
{
    // radius should always be <= extent.Length() (the circumsphere of the new box)
    BoundingBoxSphere box(Vector3(0, 0, 0), Vector3(1, 2, 3));

    Matrix4 mat = Matrix4::Identity();
    mat[0][0] = 0.5f;
    mat[1][1] = 4.f;
    mat[2][2] = 1.f;

    auto result = BoundingBoxSphere::Transform(box, mat);

    ASSERT_LE(result.radius, result.extent.Length() + EPSILON);
}

TEST(BoundsTest, TransformOffCenterBoxTest)
{
    BoundingBoxSphere box(Vector3(5, 0, 0), Vector3(1, 1, 1));

    // Uniform scale by 2
    Matrix4 mat = Matrix4::Identity();
    mat[0][0] = 2.f;
    mat[1][1] = 2.f;
    mat[2][2] = 2.f;

    auto result = BoundingBoxSphere::Transform(box, mat);

    // Original AABB: min=(4,-1,-1) max=(6,1,1)
    // After scale: min=(8,-2,-2) max=(12,2,2)
    // center=(10, 0, 0), extent=(2, 2, 2)
    ASSERT_NEAR(result.center.x, 10.f, EPSILON);
    ASSERT_NEAR(result.center.y, 0.f, EPSILON);
    ASSERT_NEAR(result.center.z, 0.f, EPSILON);
    ASSERT_NEAR(result.extent.x, 2.f, EPSILON);
    ASSERT_NEAR(result.extent.y, 2.f, EPSILON);
    ASSERT_NEAR(result.extent.z, 2.f, EPSILON);
}

