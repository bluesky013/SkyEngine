//
// Created by Zach Lee on 2021/12/26.
//

#include <gtest/gtest.h>
#include <core/logger/Logger.h>
#include <engine/render/rendergraph/RenderGraphNode.h>

using namespace sky;

static uint32_t g_counter = 0;

struct TestNode : public RenderGraphNode {
public:
    TestNode(TestNode* node) : RenderGraphNode(node)
    {
        ++g_counter;
    }

    ~TestNode()
    {
        --g_counter;
    }
};

TEST(RenderGraphNodeTest, GraphCtrDtrTest)
{
    {
        TestNode root(nullptr);
        auto p1 = new TestNode(&root);
        auto p2 = new TestNode(&root);
        auto p3 = new TestNode(&root);

        auto pp1 = new TestNode(p1);
        auto pp2 = new TestNode(p2);
        auto pp3 = new TestNode(p3);

        auto ppp2 = new TestNode(pp2);
        ASSERT_EQ(g_counter, 8);
        delete pp2;
        ASSERT_EQ(g_counter, 6);
    }
    ASSERT_EQ(g_counter, 0);
}