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
#ifdef __APPLE__
    RHI::Get()->InitInstance({"test", "", true, rhi::API::METAL});
#else
    RHI::Get()->InitInstance({"test", "", true, rhi::API::GLES});
#endif
    RHI::Get()->InitDevice({});

    RenderGraphContext context;
    context.pool.reset(new TransientObjectPool());

    RenderGraph graph(&context);

    graph.AddImage("test", GraphImage{{128, 128, 1}, 2, 2, rhi::PixelFormat::RGBA8_UNORM, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED});
    graph.AddImageView("test_1", "test", GraphImageView{{0, 1, 0, 2}});
    graph.AddImageView("test_1_1", "test_1", GraphImageView{{0, 1, 0, 1}});
    graph.AddImageView("test_1_2", "test_1", GraphImageView{{0, 1, 1, 1}});
    graph.AddImageView("test_2", "test",GraphImageView{1, 1, 0, 2});
    graph.AddImageView("test_2_1", "test_2",GraphImageView{1, 1, 0, 1});
    graph.AddImageView("test_2_2", "test_2",GraphImageView{1, 1, 1, 1});
    graph.AddImage("test2", GraphImage{{128, 128, 1}, 1, 1, rhi::PixelFormat::D24_S8, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED});

    graph.AddRasterPass("color0", 128, 128);
    graph.AddRasterSubPass("sub0", "color0")
        .AddRasterView("test_1_1", {RenderTargetType::COLOR, ResourceAccessBit::WRITE, rhi::ClearValue(0.f, 0.f, 0.f, 0.f), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE})
        .AddRasterView("test_1_2", {RenderTargetType::COLOR, ResourceAccessBit::WRITE, rhi::ClearValue(0.f, 0.f, 0.f, 0.f), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE})
        .AddRasterView("test_2_1", {RenderTargetType::COLOR, ResourceAccessBit::WRITE, rhi::ClearValue(0.f, 0.f, 0.f, 0.f), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE})
        .AddRasterView("test_2_2", {RenderTargetType::COLOR, ResourceAccessBit::WRITE, rhi::ClearValue(0.f, 0.f, 0.f, 0.f), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE})
        .AddRasterView("test2", {RenderTargetType::DEPTH_STENCIL, ResourceAccessBit::WRITE, rhi::ClearValue(1.f, 0), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE});

    {
        RenderResourceCompiler compiler(graph);
        PmrVector<boost::default_color_type> colors(graph.vertices.size(), &graph.context->resources);
        boost::depth_first_search(graph.resourceGraph, compiler, ColorMap(colors));
    }
    {
        RenderGraphPassCompiler compiler(graph);
        PmrVector<boost::default_color_type> colors(graph.vertices.size(), &graph.context->resources);
        boost::depth_first_search(graph.passGraph, compiler, ColorMap(colors));
    }
    {
        RenderDependencyCompiler compiler(graph);
        PmrVector<boost::default_color_type> colors(graph.vertices.size(), &graph.context->resources);
        boost::depth_first_search(graph.dependencyGraph, compiler, ColorMap(colors));
    }
}
