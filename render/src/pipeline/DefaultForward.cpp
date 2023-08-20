//
// Created by Zach Lee on 2023/8/20.
//

#include <render/pipeline/DefaultForward.h>
#include <render/rdg/RenderGraph.h>
#include <render/RenderWindow.h>

namespace sky {

    void DefaultForward::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
    }

    void DefaultForward::OnSetup(rdg::RenderGraph &rdg)
    {
        const auto &swapchain = output->GetSwaChain();
        const auto &ext = swapchain->GetExtent();
        const auto width  = ext.width;
        const auto height = ext.height;
        const auto format = swapchain->GetFormat();

        auto &rg = rdg.resourceGraph;
        rg.ImportImage("ForwardColor", swapchain->GetImage(0));
        rg.AddImage("ForwardDepthStencil", rdg::GraphImage{{width, height, 1}, 1, 1, rhi::PixelFormat::D24_S8, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED});

        auto forwardPass = rdg.AddRasterPass("forwardColor", width, height)
                         .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.f, 0.f, 0.f, 0.f))
                         .AddAttachment({"ForwardDepthStencil", rhi::LoadOp::CLEAR, rhi::StoreOp::DONT_CARE}, rhi::ClearValue(1.f, 0));

        forwardPass.AddRasterSubPass("color0_sub0")
            .AddColor("ForwardColor", rdg::ResourceAccessBit::WRITE)
            .AddDepthStencil("ForwardDepthStencil", rdg::ResourceAccessBit::WRITE)
            .AddSceneView("queue1", nullptr);

        rdg.AddPresentPass("present", swapchain);
    }

} // namespace sky