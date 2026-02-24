//
// Created by blues on 2024/10/18.
//

#include <core/tree/OctTree.h>
#include <core/tree/BVH.h>
#include <core/shapes/AABB.h>
#include <core/template/ReferenceObject.h>
#include <core/shapes/Shapes.h>
#include <gtest/gtest.h>

using namespace sky;

struct TestOctElement;
using TestOctElementRef = CounterPtr<TestOctElement>;

template <>
struct OctreeTraits<TestOctElementRef> {
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
    Octree<TestOctElementRef> octree(2.f);

    TestOctElementRef element1 = new TestOctElement(1);
    TestOctElementRef element2 = new TestOctElement(2);
    TestOctElementRef element3 = new TestOctElement(3);
    TestOctElementRef element4 = new TestOctElement(4);
    TestOctElementRef element5 = new TestOctElement(5);
    {

        Vector3 c1 = Vector3(0.25, -0.25, 0.25);
        element1->boundingBox = AABB{c1 - Vector3(0.1f), c1 + Vector3(0.1f)};
        octree.AddElement(element1);
    }

    {
        Vector3 c1 = Vector3(-0.25, 0.25, 0.25);
        element2->boundingBox = AABB{c1 - Vector3(0.1f), c1 + Vector3(0.1f)};
        octree.AddElement(element2);
    }

    {
        Vector3 c1 = Vector3(0.125, 0.125, 0.125);
        element3->boundingBox = AABB{c1 - Vector3(0.05f), c1 + Vector3(0.05f)};
        octree.AddElement(element3);
    }

    {
        Vector3 c1 = Vector3(0.125, 0.125, 0.125);
        element4->boundingBox = AABB{c1 - Vector3(0.05f), c1 + Vector3(0.05f)};
        octree.AddElement(element4);
    }

    {
        Vector3 c1 = Vector3(0.125, 0.125, 0.125);
        element5->boundingBox = AABB{c1 - Vector3(0.05f), c1 + Vector3(0.05f)};
        octree.AddElement(element5);
    }

    octree.RemoveElement(element2->index);

    std::vector<TestOctElementRef> result;
    octree.ForeachWithBoundTest(AABB{VEC3_ZERO, VEC3_ONE}, [&result](const TestOctElementRef &ele) {
        result.emplace_back(ele);
    });
    ASSERT_EQ(result.size(), 3);
    ASSERT_EQ(result[0]->id, 3);
    ASSERT_EQ(result[1]->id, 4);
    ASSERT_EQ(result[2]->id, 5);
}
// ============================================================================
// BVH Tests
// ============================================================================

struct TestBVHElement {
    uint32_t id;
    AABB bounds;
};

template <>
struct BVHTraits<TestBVHElement> {
    static AABB GetBounds(const TestBVHElement &element)
    {
        return element.bounds;
    }
};

TEST(BVHTest, BuildEmpty)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    bvh.Build(elements);
    
    ASSERT_TRUE(bvh.IsEmpty());
    ASSERT_EQ(bvh.GetNodeCount(), 0);
    ASSERT_EQ(bvh.GetElementCount(), 0);
}

TEST(BVHTest, BuildSingleElement)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    elements.push_back({1, AABB{Vector3(0.f), Vector3(1.f)}});
    
    bvh.Build(elements);
    
    ASSERT_FALSE(bvh.IsEmpty());
    ASSERT_EQ(bvh.GetNodeCount(), 1);
    ASSERT_EQ(bvh.GetElementCount(), 1);
    ASSERT_TRUE(bvh.GetRoot().IsLeaf());
}

TEST(BVHTest, BuildMultipleElements)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    // Create a grid of elements
    for (int i = 0; i < 10; ++i) {
        float x = static_cast<float>(i);
        elements.push_back({static_cast<uint32_t>(i), 
            AABB{Vector3(x, 0.f, 0.f), Vector3(x + 1.f, 1.f, 1.f)}});
    }
    
    bvh.Build(elements);
    
    ASSERT_FALSE(bvh.IsEmpty());
    ASSERT_EQ(bvh.GetElementCount(), 10);
    ASSERT_GT(bvh.GetNodeCount(), 1);  // Should have internal nodes
}

TEST(BVHTest, QueryAABB)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    // Create elements at different positions
    elements.push_back({1, AABB{Vector3(0.f), Vector3(1.f)}});
    elements.push_back({2, AABB{Vector3(5.f), Vector3(6.f)}});
    elements.push_back({3, AABB{Vector3(10.f), Vector3(11.f)}});
    elements.push_back({4, AABB{Vector3(0.5f, 0.5f, 0.5f), Vector3(1.5f, 1.5f, 1.5f)}});
    
    bvh.Build(elements);
    
    // Query area around first two elements
    std::vector<const TestBVHElement*> result;
    bvh.QueryAABB(AABB{Vector3(-1.f), Vector3(2.f)}, result);
    
    ASSERT_EQ(result.size(), 2);  // Should find elements 1 and 4
}

TEST(BVHTest, QueryAABBCallback)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    for (int i = 0; i < 5; ++i) {
        float x = static_cast<float>(i * 2);
        elements.push_back({static_cast<uint32_t>(i), 
            AABB{Vector3(x, 0.f, 0.f), Vector3(x + 1.f, 1.f, 1.f)}});
    }
    
    bvh.Build(elements);
    
    // Query overlapping middle elements
    std::vector<uint32_t> foundIds;
    bvh.QueryAABB(AABB{Vector3(3.5f, -1.f, -1.f), Vector3(6.5f, 2.f, 2.f)}, 
        [&foundIds](const TestBVHElement &elem) {
            foundIds.push_back(elem.id);
        });
    
    ASSERT_EQ(foundIds.size(), 2);  // Should find elements at x=4 and x=6
}

TEST(BVHTest, RayCast)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    // Create elements in a row
    elements.push_back({1, AABB{Vector3(5.f, -0.5f, -0.5f), Vector3(6.f, 0.5f, 0.5f)}});
    elements.push_back({2, AABB{Vector3(10.f, -0.5f, -0.5f), Vector3(11.f, 0.5f, 0.5f)}});
    elements.push_back({3, AABB{Vector3(15.f, -0.5f, -0.5f), Vector3(16.f, 0.5f, 0.5f)}});
    
    bvh.Build(elements);
    
    // Cast ray from origin along X axis
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    auto hit = bvh.RayCast(ray);
    
    ASSERT_TRUE(hit.hit);
    ASSERT_EQ(hit.element->id, 1);  // First element along the ray
    ASSERT_NEAR(hit.distance, 5.f, 0.01f);
}

TEST(BVHTest, RayCastMiss)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    elements.push_back({1, AABB{Vector3(5.f, 5.f, 5.f), Vector3(6.f, 6.f, 6.f)}});
    
    bvh.Build(elements);
    
    // Cast ray that misses
    Ray ray{Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};
    auto hit = bvh.RayCast(ray);
    
    ASSERT_FALSE(hit.hit);
}

TEST(BVHTest, QueryPoint)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    elements.push_back({1, AABB{Vector3(0.f), Vector3(2.f)}});
    elements.push_back({2, AABB{Vector3(1.f), Vector3(3.f)}});  // Overlaps with first
    elements.push_back({3, AABB{Vector3(10.f), Vector3(12.f)}});
    
    bvh.Build(elements);
    
    // Query point inside overlapping region
    std::vector<uint32_t> foundIds;
    bvh.QueryPoint(Vector3(1.5f, 1.5f, 1.5f), [&foundIds](const TestBVHElement &elem) {
        foundIds.push_back(elem.id);
    });
    
    ASSERT_EQ(foundIds.size(), 2);  // Should find both overlapping elements
}

TEST(BVHTest, GetDepth)
{
    BVH<TestBVHElement> bvh;
    std::vector<TestBVHElement> elements;
    
    // Single element
    elements.push_back({1, AABB{Vector3(0.f), Vector3(1.f)}});
    bvh.Build(elements);
    ASSERT_EQ(bvh.GetDepth(), 1);
    
    // Many elements should create deeper tree
    elements.clear();
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(i);
        elements.push_back({static_cast<uint32_t>(i), 
            AABB{Vector3(x, 0.f, 0.f), Vector3(x + 0.5f, 0.5f, 0.5f)}});
    }
    bvh.Build(elements);
    ASSERT_GT(bvh.GetDepth(), 1);
}

TEST(BVHTest, BuildStrategies)
{
    std::vector<TestBVHElement> elements;
    for (int i = 0; i < 50; ++i) {
        float x = static_cast<float>(i);
        elements.push_back({static_cast<uint32_t>(i), 
            AABB{Vector3(x, 0.f, 0.f), Vector3(x + 0.5f, 0.5f, 0.5f)}});
    }
    
    // Test different strategies
    BVH<TestBVHElement> bvhMedian;
    BVHConfig configMedian;
    configMedian.strategy = BVHBuildStrategy::MEDIAN;
    bvhMedian.Build(elements, configMedian);
    ASSERT_EQ(bvhMedian.GetElementCount(), 50);
    
    BVH<TestBVHElement> bvhObjectMedian;
    BVHConfig configObject;
    configObject.strategy = BVHBuildStrategy::OBJECT_MEDIAN;
    bvhObjectMedian.Build(elements, configObject);
    ASSERT_EQ(bvhObjectMedian.GetElementCount(), 50);
    
    BVH<TestBVHElement> bvhSAH;
    BVHConfig configSAH;
    configSAH.strategy = BVHBuildStrategy::SAH;
    bvhSAH.Build(elements, configSAH);
    ASSERT_EQ(bvhSAH.GetElementCount(), 50);
}

TEST(BVHTest, AABBElements)
{
    // Test direct AABB elements (using specialization)
    BVH<AABB> bvh;
    std::vector<AABB> elements;
    
    elements.push_back(AABB{Vector3(0.f), Vector3(1.f)});
    elements.push_back(AABB{Vector3(2.f), Vector3(3.f)});
    elements.push_back(AABB{Vector3(4.f), Vector3(5.f)});
    
    bvh.Build(elements);
    
    ASSERT_EQ(bvh.GetElementCount(), 3);
    
    std::vector<const AABB*> result;
    bvh.QueryAABB(AABB{Vector3(-1.f), Vector3(1.5f)}, result);
    ASSERT_EQ(result.size(), 1);
}
