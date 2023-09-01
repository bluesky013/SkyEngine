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
