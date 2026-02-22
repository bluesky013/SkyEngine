//
// Created by blues on 2024/10/18.
//

#include <core/tree/OctTree.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>
#include <core/shapes/Shapes.h>
#include <gtest/gtest.h>

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