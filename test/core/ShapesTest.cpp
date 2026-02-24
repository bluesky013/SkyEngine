//
// Created by Zach Lee on 2023/8/30.
//

#include <core/shapes/Shapes.h>
#include <core/shapes/Frustum.h>
#include <core/math/MathUtil.h>
#include <gtest/gtest.h>

using namespace sky;
TEST(ShapesTest, ViewFrustumTest)
{
    const auto mtx = MakePerspective(90.f / 180.f * 3.14f, 1.0, 0.1f, 100.f);

    {
        Vector3    center{0, 0, -5};
        const auto aabb1 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        center.z         = 5;
        const auto aabb2 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        center.z         = -150;
        const auto aabb3 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_TRUE(Intersection(aabb1, CreateFrustumByViewProjectMatrix(mtx)));
        ASSERT_FALSE(Intersection(aabb2, CreateFrustumByViewProjectMatrix(mtx)));
        ASSERT_FALSE(Intersection(aabb3, CreateFrustumByViewProjectMatrix(mtx)));
    }

    {
        Vector3 center{0, 5.9f, -4.f};
        const auto aabb1 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_TRUE(Intersection(aabb1, CreateFrustumByViewProjectMatrix(mtx)));
        center.y = 6.1f;
        const auto aabb2 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_FALSE(Intersection(aabb2, CreateFrustumByViewProjectMatrix(mtx)));

        center.y = -5.9f;
        const auto aabb3 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_TRUE(Intersection(aabb3, CreateFrustumByViewProjectMatrix(mtx)));

        center.y = -6.1f;
        const auto aabb4 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_FALSE(Intersection(aabb4, CreateFrustumByViewProjectMatrix(mtx)));
    }

    {
        Vector3 center{5.9f, 0, -4};
        const auto aabb1 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_TRUE(Intersection(aabb1, CreateFrustumByViewProjectMatrix(mtx)));
        center.x = 6.1f;
        const auto aabb2 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_FALSE(Intersection(aabb2, CreateFrustumByViewProjectMatrix(mtx)));

        center.x = -5.9f;
        const auto aabb3 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_TRUE(Intersection(aabb3, CreateFrustumByViewProjectMatrix(mtx)));

        center.x = -6.1f;
        const auto aabb4 = AABB{center - Vector3{1, 1, 1}, center + Vector3{1, 1, 1}};
        ASSERT_FALSE(Intersection(aabb4, CreateFrustumByViewProjectMatrix(mtx)));
    }
}

// ============================================================================
// Ray-AABB Intersection Tests
// ============================================================================

TEST(ShapesTest, RayAABBIntersectionHit)
{
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    AABB aabb{Vector3(5.f, -1.f, -1.f), Vector3(7.f, 1.f, 1.f)};
    
    auto [hit, tMin, tMax] = Intersection(ray, aabb);
    
    ASSERT_TRUE(hit);
    ASSERT_NEAR(tMin, 5.f, 0.001f);
    ASSERT_NEAR(tMax, 7.f, 0.001f);
}

TEST(ShapesTest, RayAABBIntersectionMiss)
{
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    AABB aabb{Vector3(5.f, 5.f, 5.f), Vector3(7.f, 7.f, 7.f)};  // Not on ray path
    
    auto [hit, tMin, tMax] = Intersection(ray, aabb);
    
    ASSERT_FALSE(hit);
}

TEST(ShapesTest, RayAABBIntersectionInsideBox)
{
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    AABB aabb{Vector3(-1.f, -1.f, -1.f), Vector3(1.f, 1.f, 1.f)};  // Ray starts inside
    
    auto [hit, tMin, tMax] = Intersection(ray, aabb);
    
    ASSERT_TRUE(hit);
    ASSERT_GE(tMin, 0.f);  // tMin should be 0 since ray starts inside
}

TEST(ShapesTest, RayAABBIntersectionBehind)
{
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    AABB aabb{Vector3(-7.f, -1.f, -1.f), Vector3(-5.f, 1.f, 1.f)};  // Behind ray
    
    auto [hit, tMin, tMax] = Intersection(ray, aabb);
    
    // Ray still intersects, but at negative t
    ASSERT_TRUE(hit);
    ASSERT_LT(tMax, 0.f);
}

TEST(ShapesTest, RayAABBIntersectionTest)
{
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    AABB aabbHit{Vector3(5.f, -1.f, -1.f), Vector3(7.f, 1.f, 1.f)};
    AABB aabbMiss{Vector3(5.f, 5.f, 5.f), Vector3(7.f, 7.f, 7.f)};
    
    ASSERT_TRUE(IntersectionTest(ray, aabbHit));
    ASSERT_FALSE(IntersectionTest(ray, aabbMiss));
}

TEST(ShapesTest, RayAABBDiagonal)
{
    // Ray going diagonally
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 1.f, 1.f)};
    ray.dir.Normalize();
    
    AABB aabb{Vector3(4.f, 4.f, 4.f), Vector3(6.f, 6.f, 6.f)};
    
    auto [hit, tMin, tMax] = Intersection(ray, aabb);
    
    ASSERT_TRUE(hit);
}
