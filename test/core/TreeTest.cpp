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

    printf("test\n");
}