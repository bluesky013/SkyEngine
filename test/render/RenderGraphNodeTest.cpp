//
// Created by Zach Lee on 2021/12/26.
//

#include <gtest/gtest.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>
#include <render/rdg/RenderGraphVisitors.h>
#include <render/rdg/RenderGraphExecutor.h>
#include <render/rdg/AccessGraphCompiler.h>
#include <render/rdg/RenderResourceCompiler.h>
#include <render/rdg/TransientObjectPool.h>

#include <memory>

using namespace sky;
using namespace sky::rdg;

TEST(RenderGraphTest, NodeGraphTest01)
{
//#ifdef __APPLE__
//    RHI::Get()->InitInstance({"test", "", true, rhi::API::METAL});
//#else
//    RHI::Get()->InitInstance({"test", "", true, rhi::API::VULKAN});
//#endif
//    RHI::Get()->InitDevice({});
//
//    RenderGraphContext context(1);
//    context.pool = std::make_unique<TransientObjectPool>();
//    context.device = RHI::Get()->GetDevice();
//    context.commandBuffers.resize(1);
//    context.commandBuffers[0] = context.device->CreateCommandBuffer({});
//
//    RenderGraph rdg(&context);
//    auto       &rg = rdg.resourceGraph;
//
//
//    /**
//     *                    color0            color1           color2
//     *                         [                  ]
//     *  test                                     R
//     *    test_1
//     *      test_1_1           W
//     *      test_1_2           W
//     *    test_2
//     *      test_2_1           W
//     *      test_2_2           W
//     *                         [                                   ]
//     *  test2                  W                RW               RW
//     */
//
//
//    rg.AddImage(Name("test"),
//        GraphImage{{128, 128, 1}, 1, 2, rhi::PixelFormat::RGBA8_UNORM, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X1, rhi::ImageViewType::VIEW_2D_ARRAY});
//    rg.AddImageView(Name("test_1"), Name("test"), GraphImageView{{0, 1, 0, 2, rhi::AspectFlagBit::COLOR_BIT, rhi::ImageViewType::VIEW_2D_ARRAY}});
//    rg.AddImageView(Name("test_1_1"), Name("test_1"), GraphImageView{{0, 1, 0, 1, rhi::AspectFlagBit::COLOR_BIT}});
//    rg.AddImageView(Name("test_1_2"), Name("test_1"), GraphImageView{{0, 1, 1, 1, rhi::AspectFlagBit::COLOR_BIT}});
//    rg.AddImage(Name("test2"),
//                GraphImage{{128, 128, 1}, 1, 1, rhi::PixelFormat::D24_S8, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED});
//
//    auto pass1 = rdg.AddRasterPass(Name("color0"), 128, 128)
//        .AddAttachment({Name("test_1_1"), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.f, 0.f, 0.f, 0.f))
//        .AddAttachment({Name("test_1_2"), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.f, 0.f, 0.f, 0.f))
//        .AddAttachment({Name("test2"), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));
//    pass1.AddRasterSubPass(Name("color0_sub0"))
//        .AddColor(Name("test_1_1"), ResourceAccessBit::WRITE)
//        .AddColor(Name("test_1_2"), ResourceAccessBit::WRITE)
//        .AddDepthStencil(Name("test2"), ResourceAccessBit::WRITE)
//        .AddQueue(Name("queue1"));
//
//    auto pass2 = rdg.AddRasterPass(Name("color1"), 128, 128)
//        .AddAttachment({Name("test2"), rhi::LoadOp::LOAD, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));
//    pass2.AddRasterSubPass(Name("color1_sub0"))
//        .AddDepthStencil(Name("test2"), ResourceAccessBit::WRITE)
//        .AddComputeView(Name("test"), {Name("_"), ComputeType::SRV, rhi::ShaderStageFlagBit::FS, ResourceAccessBit::READ});
//
//    auto pass3 = rdg.AddRasterPass(Name("color2"), 128, 128)
//        .AddAttachment({Name("test2"), rhi::LoadOp::LOAD, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));
//    pass3.AddRasterSubPass(Name("color2_sub0"))
//        .AddDepthStencil(Name("test2"), ResourceAccessBit::WRITE);
//
//    {
//        AccessCompiler             compiler(rdg);
//        PmrVector<boost::default_color_type> colors(rdg.accessGraph.vertices.size(), &rdg.context->resources);
//        boost::depth_first_search(rdg.accessGraph.graph, compiler, ColorMap(colors));
//    }
//
//
//    {
//        RenderResourceCompiler               compiler(rdg);
//        PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
//        boost::depth_first_search(rdg.graph, compiler, ColorMap(colors));
//    }
//
//    {
//        RenderGraphPassCompiler              compiler(rdg);
//        PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
//        boost::depth_first_search(rdg.graph, compiler, ColorMap(colors));
//    }
//
//    {
//        context.MainCommandBuffer()->Begin();
//        RenderGraphExecutor executor(rdg);
//        PmrVector<boost::default_color_type> colors(rdg.vertices.size(), &rdg.context->resources);
//        boost::depth_first_search(rdg.graph, executor, ColorMap(colors));
//        context.MainCommandBuffer()->End();
//
//        auto *queue = context.device->GetQueue(rhi::QueueType::GRAPHICS);
//        context.MainCommandBuffer()->Submit(*queue, {});
//    }
//    context.device->WaitIdle();
}
