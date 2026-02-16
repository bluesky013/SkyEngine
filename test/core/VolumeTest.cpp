//
// Tests for volume management system
//

#include <core/shapes/Shapes.h>
#include <core/shapes/AABB.h>
#include <core/shapes/BoundingVolume.h>
#include <core/shapes/Frustum.h>
#include <core/math/MathUtil.h>
#include <core/tree/VolumeManager.h>
#include <gtest/gtest.h>

using namespace sky;

// ============================================
// AABB Helper Tests
// ============================================
TEST(VolumeTest, AABBHelpers)
{
    AABB aabb(Vector3(-1, -2, -3), Vector3(1, 2, 3));

    auto center = aabb.GetCenter();
    EXPECT_FLOAT_EQ(center.x, 0.f);
    EXPECT_FLOAT_EQ(center.y, 0.f);
    EXPECT_FLOAT_EQ(center.z, 0.f);

    auto extent = aabb.GetExtent();
    EXPECT_FLOAT_EQ(extent.x, 1.f);
    EXPECT_FLOAT_EQ(extent.y, 2.f);
    EXPECT_FLOAT_EQ(extent.z, 3.f);

    EXPECT_TRUE(aabb.IsValid());
    EXPECT_TRUE(aabb.Contains(Vector3(0, 0, 0)));
    EXPECT_TRUE(aabb.Contains(Vector3(1, 2, 3)));
    EXPECT_FALSE(aabb.Contains(Vector3(1.1f, 0, 0)));

    AABB invalid(Vector3(1, 0, 0), Vector3(-1, 0, 0));
    EXPECT_FALSE(invalid.IsValid());
}

// ============================================
// Sphere-Plane Intersection Tests
// ============================================
TEST(VolumeTest, SpherePlaneIntersection)
{
    Plane plane{VEC3_Y, 0.f};

    // Sphere intersects plane
    Sphere sphere1{Vector3(0, 0.5f, 0), 1.f};
    auto [intersects1, side1] = Intersection(sphere1, plane);
    EXPECT_TRUE(intersects1);
    EXPECT_EQ(side1, 0);

    // Sphere fully above plane
    Sphere sphere2{Vector3(0, 5, 0), 1.f};
    auto [intersects2, side2] = Intersection(sphere2, plane);
    EXPECT_FALSE(intersects2);
    EXPECT_EQ(side2, 1);

    // Sphere fully below plane
    Sphere sphere3{Vector3(0, -5, 0), 1.f};
    auto [intersects3, side3] = Intersection(sphere3, plane);
    EXPECT_FALSE(intersects3);
    EXPECT_EQ(side3, -1);
}

// ============================================
// Sphere-AABB Intersection Tests
// ============================================
TEST(VolumeTest, SphereAABBIntersection)
{
    AABB aabb(Vector3(-1, -1, -1), Vector3(1, 1, 1));

    // Sphere centered at origin
    Sphere sphere1{VEC3_ZERO, 0.5f};
    EXPECT_TRUE(Intersection(sphere1, aabb));

    // Sphere barely touching
    Sphere sphere2{Vector3(2, 0, 0), 1.f};
    EXPECT_TRUE(Intersection(sphere2, aabb));

    // Sphere not touching
    Sphere sphere3{Vector3(3, 0, 0), 1.f};
    EXPECT_FALSE(Intersection(sphere3, aabb));

    // Sphere at corner
    Sphere sphere4{Vector3(2, 2, 2), 1.73f};
    EXPECT_TRUE(Intersection(sphere4, aabb));
}

// ============================================
// Sphere-Frustum Intersection Tests
// ============================================
TEST(VolumeTest, SphereFrustumIntersection)
{
    const auto mtx = MakePerspective(90.f / 180.f * 3.14f, 1.0, 0.1f, 100.f);
    const auto frustum = CreateFrustumByViewProjectMatrix(mtx);

    // Inside frustum
    Sphere sphere1{Vector3(0, 0, -5), 1.f};
    EXPECT_TRUE(Intersection(sphere1, frustum));

    // Behind camera
    Sphere sphere2{Vector3(0, 0, 5), 1.f};
    EXPECT_FALSE(Intersection(sphere2, frustum));

    // Beyond far plane
    Sphere sphere3{Vector3(0, 0, -150), 1.f};
    EXPECT_FALSE(Intersection(sphere3, frustum));
}

// ============================================
// BoundingVolume Tests
// ============================================
TEST(VolumeTest, BoundingVolumeAABB)
{
    AABB aabb(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    BoundingVolume vol(aabb);

    EXPECT_EQ(vol.GetType(), BoundingVolume::Type::BOX);
    EXPECT_FLOAT_EQ(vol.GetAABB().min.x, -1.f);

    auto converted = vol.ToAABB();
    EXPECT_FLOAT_EQ(converted.min.x, -1.f);
    EXPECT_FLOAT_EQ(converted.max.x, 1.f);

    AABB other(Vector3(0, 0, 0), Vector3(2, 2, 2));
    EXPECT_TRUE(vol.IntersectsAABB(other));

    AABB noOverlap(Vector3(5, 5, 5), Vector3(6, 6, 6));
    EXPECT_FALSE(vol.IntersectsAABB(noOverlap));
}

TEST(VolumeTest, BoundingVolumeSphere)
{
    Sphere sphere{VEC3_ZERO, 2.f};
    BoundingVolume vol(sphere);

    EXPECT_EQ(vol.GetType(), BoundingVolume::Type::SPHERE);

    auto converted = vol.ToAABB();
    EXPECT_FLOAT_EQ(converted.min.x, -2.f);
    EXPECT_FLOAT_EQ(converted.max.x, 2.f);

    AABB other(Vector3(1, 0, 0), Vector3(3, 1, 1));
    EXPECT_TRUE(vol.IntersectsAABB(other));
}

TEST(VolumeTest, BoundingVolumeFrustumCulling)
{
    const auto mtx = MakePerspective(90.f / 180.f * 3.14f, 1.0, 0.1f, 100.f);
    const auto frustum = CreateFrustumByViewProjectMatrix(mtx);

    BoundingVolume boxVol(AABB(Vector3(-1, -1, -6), Vector3(1, 1, -4)));
    EXPECT_TRUE(boxVol.IntersectsFrustum(frustum));

    BoundingVolume sphereVol(Sphere{Vector3(0, 0, -5), 1.f});
    EXPECT_TRUE(sphereVol.IntersectsFrustum(frustum));

    BoundingVolume behindVol(AABB(Vector3(-1, -1, 4), Vector3(1, 1, 6)));
    EXPECT_FALSE(behindVol.IntersectsFrustum(frustum));
}

// ============================================
// VolumeManager Tests
// ============================================
TEST(VolumeTest, VolumeManagerAddRemove)
{
    VolumeManager mgr(1024.f);

    AABB aabb1(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    AABB aabb2(Vector3(10, 10, 10), Vector3(12, 12, 12));

    auto id1 = mgr.AddVolume(aabb1);
    auto id2 = mgr.AddVolume(aabb2);

    EXPECT_EQ(mgr.GetVolumeCount(), 2u);
    EXPECT_NE(id1, id2);

    auto *found = mgr.FindVolume(id1);
    ASSERT_NE(found, nullptr);
    EXPECT_FLOAT_EQ(found->worldBound.min.x, -1.f);

    mgr.RemoveVolume(id1);
    EXPECT_EQ(mgr.GetVolumeCount(), 1u);
    EXPECT_EQ(mgr.FindVolume(id1), nullptr);

    auto *found2 = mgr.FindVolume(id2);
    ASSERT_NE(found2, nullptr);
}

TEST(VolumeTest, VolumeManagerUpdate)
{
    VolumeManager mgr(1024.f);

    AABB aabb(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto id = mgr.AddVolume(aabb);

    AABB newAABB(Vector3(5, 5, 5), Vector3(7, 7, 7));
    mgr.UpdateVolume(id, newAABB);

    auto *found = mgr.FindVolume(id);
    ASSERT_NE(found, nullptr);
    EXPECT_FLOAT_EQ(found->worldBound.min.x, 5.f);
    EXPECT_FLOAT_EQ(found->worldBound.max.x, 7.f);
}

TEST(VolumeTest, VolumeManagerFrustumCull)
{
    VolumeManager mgr(1024.f);

    // Add some volumes at different positions
    AABB inside(Vector3(-1, -1, -6), Vector3(1, 1, -4));
    AABB behind(Vector3(-1, -1, 4), Vector3(1, 1, 6));
    AABB farAway(Vector3(-1, -1, -200), Vector3(1, 1, -198));

    mgr.AddVolume(inside);
    mgr.AddVolume(behind);
    mgr.AddVolume(farAway);

    const auto mtx = MakePerspective(90.f / 180.f * 3.14f, 1.0, 0.1f, 100.f);
    const auto frustum = CreateFrustumByViewProjectMatrix(mtx);

    std::vector<VolumeID> visibleIds;
    mgr.FrustumCull(frustum, [&visibleIds](const VolumeEntry &entry) {
        visibleIds.push_back(entry.id);
    });

    EXPECT_EQ(visibleIds.size(), 1u);
}

TEST(VolumeTest, VolumeManagerAABBQuery)
{
    VolumeManager mgr(1024.f);

    AABB vol1(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    AABB vol2(Vector3(10, 10, 10), Vector3(12, 12, 12));
    AABB vol3(Vector3(0, 0, 0), Vector3(3, 3, 3));

    mgr.AddVolume(vol1);
    mgr.AddVolume(vol2);
    mgr.AddVolume(vol3);

    AABB queryBox(Vector3(-2, -2, -2), Vector3(2, 2, 2));
    std::vector<VolumeID> hitIds;
    mgr.QueryByAABB(queryBox, [&hitIds](const VolumeEntry &entry) {
        hitIds.push_back(entry.id);
    });

    EXPECT_EQ(hitIds.size(), 2u);
}

TEST(VolumeTest, VolumeManagerUserData)
{
    VolumeManager mgr(1024.f);

    int myData = 42;
    AABB aabb(Vector3(-1, -1, -1), Vector3(1, 1, 1));
    auto id = mgr.AddVolume(aabb, &myData);

    auto *found = mgr.FindVolume(id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*static_cast<int*>(found->userData), 42);
}
