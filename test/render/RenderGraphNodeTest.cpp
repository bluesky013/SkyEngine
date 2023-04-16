//
// Created by Zach Lee on 2021/12/26.
//

#include <core/logger/Logger.h>
#include <gtest/gtest.h>
#include <render/rdg/RenderGraph.h>

using namespace sky;

TEST(RenderGraphTest, NodeGraphTest01)
{
    PmrUnSyncPoolRes res;
    rdg::RenderGraph graph(&res);
    graph.Compile();

}