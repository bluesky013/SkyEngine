//
// Created by blues on 2024/10/18.
//

#include <core/tree/OctTree.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>
#include <core/shapes/Shapes.h>
#include <gtest/gtest.h>
#include <core/tree/Bvh.h>

using namespace sky;

struct TestOctElement;
using TestOctElementRef = CounterPtr<TestOctElement>;

template <>
struct sky::OctreeTraits<TestOctElementRef> {
    using TreeType = Octree<TestOctElementRef>;
    using BoundType = AABB;
    static const uint32_t MAX_DEPTH = 3;
    static const uint32_t MAX_ELEMENT_LEAF = 2;

    static const BoundType &GetBounds(const TestOctElementRef &ele);
    static void IndexChanged(const TestOctElementRef &ele, const TreeType::ElementIndex &index);
};

using TreeType = Octree<TestOctElementRef>;

struct TestOctElement : RefObject {
    explicit TestOctElement(uint32_t id_) : id(id_) {}

    uint32_t id;
    AABB boundingBox;
    TreeType::ElementIndex index;
};

const OctreeTraits<TestOctElementRef>::BoundType &OctreeTraits<TestOctElementRef>::GetBounds(const TestOctElementRef &ele)
{
    return ele->boundingBox;
}

void OctreeTraits<TestOctElementRef>::IndexChanged(const TestOctElementRef &ele, const TreeType::ElementIndex &index)
{
    ele->index = index;
}

TEST(OctreeTest, BasicTest)
{
    // ...existing code...
}

TEST(OctreeTest, AddAndRemoveElements)
{
    Octree<TestOctElementRef> octree(2.f);

    std::vector<TestOctElementRef> elements;
    for (uint32_t i = 0; i < 5; ++i) {
        auto ele = new TestOctElement(i);
        Vector3 center = Vector3(i * 0.1f - 0.2f, i * 0.1f - 0.2f, i * 0.1f - 0.2f);
        ele->boundingBox = AABB{center - Vector3(0.05f), center + Vector3(0.05f)};
        octree.AddElement(ele);
        elements.push_back(ele);
    }

    // Verify element count
    ASSERT_EQ(octree.GetElementCount(), 5);

    // Remove an element
    octree.RemoveElement(elements[2]->index);
    ASSERT_EQ(octree.GetElementCount(), 4);

    // Remove another element
    octree.RemoveElement(elements[0]->index);
    ASSERT_EQ(octree.GetElementCount(), 3);
}

TEST(OctreeTest, QueryByBounds)
{
    Octree<TestOctElementRef> octree(2.f);

    // Add elements at different locations
    TestOctElementRef ele1 = new TestOctElement(1);
    ele1->boundingBox = AABB{Vector3(0.3f, 0.3f, 0.3f), Vector3(0.5f, 0.5f, 0.5f)};
    octree.AddElement(ele1);

    TestOctElementRef ele2 = new TestOctElement(2);
    ele2->boundingBox = AABB{Vector3(-0.5f, -0.5f, -0.5f), Vector3(-0.3f, -0.3f, -0.3f)};
    octree.AddElement(ele2);

    TestOctElementRef ele3 = new TestOctElement(3);
    ele3->boundingBox = AABB{Vector3(0.1f, 0.1f, 0.1f), Vector3(0.3f, 0.3f, 0.3f)};
    octree.AddElement(ele3);

    // Query overlapping region (should find ele1 and ele3)
    std::vector<TestOctElementRef> result;
    AABB queryBounds{Vector3(0.2f, 0.2f, 0.2f), Vector3(0.4f, 0.4f, 0.4f)};
    octree.ForeachWithBoundTest(queryBounds, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });
    ASSERT_EQ(result.size(), 2);

    // Query non-overlapping region
    result.clear();
    AABB emptyQuery{Vector3(1.0f, 1.0f, 1.0f), Vector3(1.2f, 1.2f, 1.2f)};
    octree.ForeachWithBoundTest(emptyQuery, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });
    ASSERT_EQ(result.size(), 0);
}

TEST(OctreeTest, UpdateElement)
{
    Octree<TestOctElementRef> octree(2.f);

    TestOctElementRef ele = new TestOctElement(1);
    ele->boundingBox = AABB{Vector3(0.2f, 0.2f, 0.2f), Vector3(0.3f, 0.3f, 0.3f)};
    octree.AddElement(ele);

    auto originalIndex = ele->index;

    // Update element with new bounds (within same node)
    ele->boundingBox = AABB{Vector3(0.21f, 0.21f, 0.21f), Vector3(0.32f, 0.32f, 0.32f)};
    octree.UpdateElement(ele->index, ele);

    // Index should remain the same for local updates
    ASSERT_EQ(ele->index.nodeIndex, originalIndex.nodeIndex);

    // Query original bounds should still find the element
    std::vector<TestOctElementRef> result;
    octree.ForeachWithBoundTest(
        AABB{Vector3(0.2f, 0.2f, 0.2f), Vector3(0.33f, 0.33f, 0.33f)},
        [&result](const TestOctElementRef &ele) { result.emplace_back(ele); });
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->id, 1);
}

TEST(OctreeTest, OctreeSubdivision)
{
    Octree<TestOctElementRef> octree(2.f);

    // Add elements to trigger subdivision
    // MAX_ELEMENT_LEAF = 2, so adding 3 elements should trigger split
    std::vector<TestOctElementRef> elements;
    Vector3 baseCenter = Vector3(0.125f, 0.125f, 0.125f);

    for (uint32_t i = 0; i < 3; ++i) {
        auto ele = new TestOctElement(i);
        Vector3 offset = Vector3(i * 0.01f, i * 0.01f, i * 0.01f);
        ele->boundingBox = AABB{baseCenter + offset - Vector3(0.02f), baseCenter + offset + Vector3(0.02f)};
        octree.AddElement(ele);
        elements.push_back(ele);
    }

    // Verify all elements are still accessible
    ASSERT_EQ(octree.GetElementCount(), 3);

    // Verify we can query and find all elements
    std::vector<TestOctElementRef> result;
    AABB queryBounds{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.3f, 0.3f, 0.3f)};
    octree.ForeachWithBoundTest(queryBounds, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });
    ASSERT_EQ(result.size(), 3);
}

TEST(OctreeTest, RemoveAndRequery)
{
    Octree<TestOctElementRef> octree(2.f);

    // Add multiple elements
    TestOctElementRef ele1 = new TestOctElement(1);
    ele1->boundingBox = AABB{Vector3(0.2f, 0.2f, 0.2f), Vector3(0.3f, 0.3f, 0.3f)};
    octree.AddElement(ele1);

    TestOctElementRef ele2 = new TestOctElement(2);
    ele2->boundingBox = AABB{Vector3(0.25f, 0.25f, 0.25f), Vector3(0.35f, 0.35f, 0.35f)};
    octree.AddElement(ele2);

    TestOctElementRef ele3 = new TestOctElement(3);
    ele3->boundingBox = AABB{Vector3(0.15f, 0.15f, 0.15f), Vector3(0.25f, 0.25f, 0.25f)};
    octree.AddElement(ele3);

    // Query should find all three
    std::vector<TestOctElementRef> result;
    AABB queryBounds{Vector3(0.15f, 0.15f, 0.15f), Vector3(0.35f, 0.35f, 0.35f)};
    octree.ForeachWithBoundTest(queryBounds, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });
    ASSERT_EQ(result.size(), 3);

    // Remove middle element
    octree.RemoveElement(ele2->index);

    // Query again - should find ele1 and ele3
    result.clear();
    octree.ForeachWithBoundTest(queryBounds, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });
    ASSERT_EQ(result.size(), 2);

    // Verify which elements remain
    bool foundEle1 = false, foundEle3 = false;
    for (const auto &ele : result) {
        if (ele->id == 1) foundEle1 = true;
        if (ele->id == 3) foundEle3 = true;
    }
    ASSERT_TRUE(foundEle1 && foundEle3);
}

TEST(OctreeTest, GetStatistics)
{
    Octree<TestOctElementRef> octree(2.f);

    ASSERT_EQ(octree.GetElementCount(), 0);
    ASSERT_EQ(octree.GetNodeCount(), 1);  // Root node

    // Add elements
    for (uint32_t i = 0; i < 10; ++i) {
        auto ele = new TestOctElement(i);
        Vector3 center = Vector3(i * 0.05f - 0.225f, i * 0.05f - 0.225f, i * 0.05f - 0.225f);
        ele->boundingBox = AABB{center - Vector3(0.02f), center + Vector3(0.02f)};
        octree.AddElement(ele);
    }

    ASSERT_EQ(octree.GetElementCount(), 10);
    ASSERT_GT(octree.GetNodeCount(), 1);  // Should have subdivided
}

TEST(OctreeTest, ClearTree)
{
    Octree<TestOctElementRef> octree(2.f);

    // Add elements
    for (uint32_t i = 0; i < 5; ++i) {
        auto ele = new TestOctElement(i);
        Vector3 center = Vector3(i * 0.1f - 0.2f, i * 0.1f - 0.2f, i * 0.1f - 0.2f);
        ele->boundingBox = AABB{center - Vector3(0.05f), center + Vector3(0.05f)};
        octree.AddElement(ele);
    }

    ASSERT_EQ(octree.GetElementCount(), 5);
    ASSERT_GT(octree.GetNodeCount(), 1);

    // Clear tree
    octree.Clear();

    ASSERT_EQ(octree.GetElementCount(), 0);
    ASSERT_EQ(octree.GetNodeCount(), 1);  // Only root node remains

    // Should be able to add elements again after clear
    auto ele = new TestOctElement(99);
    ele->boundingBox = AABB{Vector3(0.2f, 0.2f, 0.2f), Vector3(0.3f, 0.3f, 0.3f)};
    octree.AddElement(ele);

    ASSERT_EQ(octree.GetElementCount(), 1);
}

TEST(OctreeTest, LargeScaleTest)
{
    Octree<TestOctElementRef> octree(10.f);

    const int ELEMENT_COUNT = 100;
    std::vector<TestOctElementRef> elements;

    // Add many elements distributed across the space
    for (int i = 0; i < ELEMENT_COUNT; ++i) {
        auto ele = new TestOctElement(i);
        float x = (i % 10) * 0.8f - 4.0f;
        float y = ((i / 10) % 10) * 0.8f - 4.0f;
        float z = (i / 100) * 0.8f;
        Vector3 center = Vector3(x, y, z);
        ele->boundingBox = AABB{center - Vector3(0.1f), center + Vector3(0.1f)};
        octree.AddElement(ele);
        elements.push_back(ele);
    }

    ASSERT_EQ(octree.GetElementCount(), ELEMENT_COUNT);

    // Query a specific region
    std::vector<TestOctElementRef> result;
    AABB queryBounds{Vector3(-1.0f, -1.0f, -0.2f), Vector3(1.0f, 1.0f, 0.2f)};
    octree.ForeachWithBoundTest(queryBounds, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });

    // Should find some elements
    ASSERT_GT(result.size(), 0);
    ASSERT_LT(result.size(), ELEMENT_COUNT);

    // Remove half of the elements
    for (int i = 0; i < ELEMENT_COUNT / 2; ++i) {
        octree.RemoveElement(elements[i]->index);
    }

    ASSERT_EQ(octree.GetElementCount(), ELEMENT_COUNT / 2);
}

TEST(OctreeTest, BoundaryConditions)
{
    Octree<TestOctElementRef> octree(2.f);

    // Test element at exact tree boundary
    TestOctElementRef ele1 = new TestOctElement(1);
    ele1->boundingBox = AABB{Vector3(-1.0f, -1.0f, -1.0f), Vector3(-0.99f, -0.99f, -0.99f)};
    octree.AddElement(ele1);

    // Test element at opposite boundary
    TestOctElementRef ele2 = new TestOctElement(2);
    ele2->boundingBox = AABB{Vector3(0.99f, 0.99f, 0.99f), Vector3(1.0f, 1.0f, 1.0f)};
    octree.AddElement(ele2);

    // Test center element
    TestOctElementRef ele3 = new TestOctElement(3);
    ele3->boundingBox = AABB{Vector3(-0.01f, -0.01f, -0.01f), Vector3(0.01f, 0.01f, 0.01f)};
    octree.AddElement(ele3);

    ASSERT_EQ(octree.GetElementCount(), 3);

    // Query should find the center element
    std::vector<TestOctElementRef> result;
    octree.ForeachWithBoundTest(
        AABB{Vector3(-0.05f, -0.05f, -0.05f), Vector3(0.05f, 0.05f, 0.05f)},
        [&result](const TestOctElementRef &ele) { result.emplace_back(ele); });
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->id, 3);
}

TEST(OctreeTest, InvalidRemoveHandling)
{
    Octree<TestOctElementRef> octree(2.f);

    TestOctElementRef ele = new TestOctElement(1);
    ele->boundingBox = AABB{Vector3(0.2f, 0.2f, 0.2f), Vector3(0.3f, 0.3f, 0.3f)};
    octree.AddElement(ele);

    auto validIndex = ele->index;

    // Remove the element
    octree.RemoveElement(validIndex);

    // Try to remove again with invalid index - should not crash
    TreeType::ElementIndex invalidIndex{validIndex.nodeIndex, validIndex.eleIndex + 10};
    octree.RemoveElement(invalidIndex);  // Should handle gracefully

    ASSERT_EQ(octree.GetElementCount(), 0);
}

// =============================================================================
// BVH test fixtures
// =============================================================================

struct TestBvhElement {
    uint32_t id;
    AABB     boundingBox;
};

template <>
struct sky::BvhTraits<TestBvhElement> {
    static const AABB &GetBounds(const TestBvhElement &ele)
    {
        return ele.boundingBox;
    }
};

using TestBvh = Bvh<TestBvhElement>;

// Helper: build a TestBvhElement centred at (cx, cy, cz) with half-extent r
static TestBvhElement MakeBvhEle(uint32_t id, float cx, float cy, float cz, float r = 0.1f)
{
    return TestBvhElement{id, AABB{Vector3(cx - r, cy - r, cz - r), Vector3(cx + r, cy + r, cz + r)}};
}

// =============================================================================
// BVH tests
// =============================================================================

TEST(BvhTest, EmptyBvh)
{
    TestBvh bvh;

    ASSERT_TRUE(bvh.Empty());
    ASSERT_EQ(bvh.GetElementCount(), 0u);
    ASSERT_EQ(bvh.GetNodeCount(), 0u);

    // Queries on empty BVH should not crash and return nothing
    std::vector<TestBvhElement> result;
    bvh.QueryAABB(AABB{Vector3(-1.f), Vector3(1.f)},
                  [&result](const TestBvhElement &e) { result.push_back(e); });
    ASSERT_EQ(result.size(), 0u);

    bvh.QueryRay(VEC3_ZERO, VEC3_Z, 1000.f,
                 [&result](const TestBvhElement &e) { result.push_back(e); });
    ASSERT_EQ(result.size(), 0u);
}

TEST(BvhTest, BuildSingleElement)
{
    TestBvh bvh;

    std::vector<TestBvhElement> elems = {MakeBvhEle(1, 0.f, 0.f, 0.f)};
    bvh.Build(elems);

    ASSERT_FALSE(bvh.Empty());
    ASSERT_EQ(bvh.GetElementCount(), 1u);
    ASSERT_GE(bvh.GetNodeCount(), 1u);

    // Root bounds should contain the single element
    const AABB &root = bvh.GetRootBounds();
    ASSERT_LE(root.min.x, -0.1f);
    ASSERT_GE(root.max.x,  0.1f);
}

TEST(BvhTest, QueryAABBBasic)
{
    TestBvh bvh;

    // Three spatially separated elements
    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1,  1.f,  0.f, 0.f),   // positive X
        MakeBvhEle(2, -1.f,  0.f, 0.f),   // negative X
        MakeBvhEle(3,  0.f,  1.f, 0.f),   // positive Y
    };
    bvh.Build(elems);

    // Query near element 1 �� should find only element 1
    std::vector<TestBvhElement> result;
    AABB query{Vector3(0.8f, -0.2f, -0.2f), Vector3(1.2f, 0.2f, 0.2f)};
    bvh.QueryAABB(query, [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].id, 1u);
}

TEST(BvhTest, QueryAABBMultipleHits)
{
    TestBvh bvh;

    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1,  0.2f, 0.f, 0.f),
        MakeBvhEle(2, -0.2f, 0.f, 0.f),
        MakeBvhEle(3,  0.f,  5.f, 0.f),   // far away
    };
    bvh.Build(elems);

    // Wide query that covers elements 1 & 2 but not 3
    std::vector<TestBvhElement> result;
    bvh.QueryAABB(AABB{Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.5f, 0.5f, 0.5f)},
                  [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_EQ(result.size(), 2u);

    bool found1 = false, found2 = false;
    for (const auto &e : result) {
        if (e.id == 1) found1 = true;
        if (e.id == 2) found2 = true;
    }
    ASSERT_TRUE(found1 && found2);
}

TEST(BvhTest, QueryAABBNoHit)
{
    TestBvh bvh;

    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1, 0.f, 0.f, 0.f),
        MakeBvhEle(2, 1.f, 0.f, 0.f),
    };
    bvh.Build(elems);

    // Query far from all elements
    std::vector<TestBvhElement> result;
    bvh.QueryAABB(AABB{Vector3(10.f, 10.f, 10.f), Vector3(11.f, 11.f, 11.f)},
                  [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_EQ(result.size(), 0u);
}

TEST(BvhTest, QueryRayHit)
{
    TestBvh bvh;

    // Elements along the +Z axis at various depths
    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1, 0.f, 0.f, 1.f),
        MakeBvhEle(2, 0.f, 0.f, 3.f),
        MakeBvhEle(3, 5.f, 5.f, 5.f),  // off to the side, not on the ray
    };
    bvh.Build(elems);

    // Ray from origin along +Z
    std::vector<TestBvhElement> result;
    bvh.QueryRay(VEC3_ZERO, VEC3_Z, 100.f,
                 [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_EQ(result.size(), 2u);

    bool found1 = false, found2 = false;
    for (const auto &e : result) {
        if (e.id == 1) found1 = true;
        if (e.id == 2) found2 = true;
    }
    ASSERT_TRUE(found1 && found2);
}

TEST(BvhTest, QueryRayNoHit)
{
    TestBvh bvh;

    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1, 5.f, 5.f, 5.f),
        MakeBvhEle(2, 5.f, 5.f, 6.f),
    };
    bvh.Build(elems);

    // Ray along +Z at the origin �� misses all elements
    std::vector<TestBvhElement> result;
    bvh.QueryRay(VEC3_ZERO, VEC3_Z, 100.f,
                 [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_EQ(result.size(), 0u);
}

TEST(BvhTest, QueryRayTMaxClip)
{
    TestBvh bvh;

    // Element along +Z axis, but beyond tMax
    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1, 0.f, 0.f,  1.f),   // at Z=1, within range
        MakeBvhEle(2, 0.f, 0.f, 10.f),   // at Z=10, beyond tMax=5
    };
    bvh.Build(elems);

    std::vector<TestBvhElement> result;
    bvh.QueryRay(VEC3_ZERO, VEC3_Z, 5.f,
                 [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].id, 1u);
}

TEST(BvhTest, RootBoundsEnclosesAll)
{
    TestBvh bvh;

    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1, -3.f, 0.f, 0.f),
        MakeBvhEle(2,  3.f, 0.f, 0.f),
        MakeBvhEle(3,  0.f, 4.f, 0.f),
    };
    bvh.Build(elems);

    const AABB &root = bvh.GetRootBounds();
    ASSERT_LE(root.min.x, -3.1f);
    ASSERT_GE(root.max.x,  3.1f);
    ASSERT_GE(root.max.y,  4.1f);
}

TEST(BvhTest, ClearAndRebuild)
{
    TestBvh bvh;

    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1, 0.f, 0.f, 0.f),
        MakeBvhEle(2, 1.f, 0.f, 0.f),
    };
    bvh.Build(elems);
    ASSERT_EQ(bvh.GetElementCount(), 2u);

    bvh.Clear();
    ASSERT_TRUE(bvh.Empty());
    ASSERT_EQ(bvh.GetElementCount(), 0u);
    ASSERT_EQ(bvh.GetNodeCount(), 0u);

    // Rebuild with a different set
    std::vector<TestBvhElement> elems2 = {MakeBvhEle(10, 2.f, 2.f, 2.f)};
    bvh.Build(elems2);
    ASSERT_EQ(bvh.GetElementCount(), 1u);

    std::vector<TestBvhElement> result;
    bvh.QueryAABB(AABB{Vector3(1.8f), Vector3(2.2f)},
                  [&result](const TestBvhElement &e) { result.push_back(e); });
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0].id, 10u);
}

TEST(BvhTest, LargeScaleAABBQuery)
{
    TestBvh bvh;

    const int N = 200;
    std::vector<TestBvhElement> elems;
    elems.reserve(N);

    for (int i = 0; i < N; ++i) {
        float x = static_cast<float>(i % 10) - 4.5f;
        float y = static_cast<float>((i / 10) % 10) - 4.5f;
        float z = static_cast<float>(i / 100);
        elems.push_back(MakeBvhEle(static_cast<uint32_t>(i), x, y, z, 0.05f));
    }
    bvh.Build(elems);

    ASSERT_EQ(bvh.GetElementCount(), static_cast<uint32_t>(N));

    // Query a 2��2��2 box around the origin
    std::vector<TestBvhElement> result;
    bvh.QueryAABB(AABB{Vector3(-1.f, -1.f, -0.5f), Vector3(1.f, 1.f, 0.5f)},
                  [&result](const TestBvhElement &e) { result.push_back(e); });

    ASSERT_GT(result.size(), 0u);
    ASSERT_LT(result.size(), static_cast<size_t>(N));
}

TEST(BvhTest, OverlappingElements)
{
    TestBvh bvh;

    // All elements occupy the same position
    std::vector<TestBvhElement> elems;
    for (uint32_t i = 0; i < 10; ++i) {
        elems.push_back(MakeBvhEle(i, 0.f, 0.f, 0.f, 0.5f));
    }
    bvh.Build(elems);

    ASSERT_EQ(bvh.GetElementCount(), 10u);

    std::vector<TestBvhElement> result;
    bvh.QueryAABB(AABB{Vector3(-1.f), Vector3(1.f)},
                  [&result](const TestBvhElement &e) { result.push_back(e); });
    ASSERT_EQ(result.size(), 10u);
}

TEST(BvhTest, QueryAllAxesRay)
{
    TestBvh bvh;

    // One element on each axis
    std::vector<TestBvhElement> elems = {
        MakeBvhEle(1,  2.f, 0.f, 0.f),   // +X axis
        MakeBvhEle(2,  0.f, 2.f, 0.f),   // +Y axis
        MakeBvhEle(3,  0.f, 0.f, 2.f),   // +Z axis
    };
    bvh.Build(elems);

    auto doQuery = [&](const Vector3 &dir, uint32_t expectedId) {
        std::vector<TestBvhElement> result;
        bvh.QueryRay(VEC3_ZERO, dir, 100.f,
                     [&result](const TestBvhElement &e) { result.push_back(e); });
        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0].id, expectedId);
    };

    doQuery(VEC3_X, 1u);
    doQuery(VEC3_Y, 2u);
    doQuery(VEC3_Z, 3u);
}