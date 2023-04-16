//
// Created by Zach Lee on 2021/12/26.
//

#include <core/logger/Logger.h>
#include <gtest/gtest.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>
#include <render/rdg/TransientObjectPool.h>
#include <render/rdg/RenderGraphCompiler.h>

using namespace sky;
using namespace sky::rdg;

TEST(RenderGraphTest, NodeGraphTest01)
{
    RHI::Get()->InitInstance({"test", "", true, rhi::API::GLES});
    RHI::Get()->InitDevice({});

    RenderGraphContext context;
    context.pool.reset(new TransientObjectPool());

    RenderGraph graph(&context);

    graph.AddImage("test", GraphImage{{128, 128, 1}, 2, 2, rhi::PixelFormat::RGBA8_UNORM, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED});
    graph.AddImageView("test_1", "test", GraphImageView{{0, 1, 0, 2}});
    graph.AddImageView("test_2", "test",GraphImageView{1, 1, 0, 2});
    graph.AddImage("test2", GraphImage{{128, 128, 1}, 1, 1, rhi::PixelFormat::RGBA8_UNORM, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED});

    RenderResourceCompiler compiler(graph);

    PmrVector<boost::default_color_type> colors(graph.vertices.size(), &graph.context->resources);
    boost::depth_first_search(graph.resourceGraph, compiler, ColorMap(colors));
}