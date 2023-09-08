//
// Created by Zach Lee on 2023/8/20.
//

#include <render/pipeline/DefaultForward.h>
#include <render/rdg/RenderGraph.h>
#include <render/RenderWindow.h>
#include <render/RenderScene.h>

namespace sky {

    void DefaultForward::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
        if (!rdgContext->device->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
            depthStencilFormat = rhi::PixelFormat::D32_S8;
        }

        rhi::DescriptorSetLayout::Descriptor desc = {};
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {
            rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::VS, "ViewInfo"
        });

        forwardLayout = std::make_shared<ResourceGroupLayout>();
        forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
        forwardLayout->AddNameHandler("ViewInfo", 0);
    }

    void DefaultForward::OnSetup(rdg::RenderGraph &rdg)
    {
        const auto &swapchain = output->GetSwaChain();
        const auto &ext = swapchain->GetExtent();
        const auto width  = ext.width;
        const auto height = ext.height;

        auto &rg = rdg.resourceGraph;

        auto &views = rdg.scene->GetSceneViews();
        if (views.empty()) {
            return;
        }
        auto *sceneView = views[0].get();
        const auto uboName = GetDefaultSceneViewUBOName(*sceneView);
        rg.ImportUBO(uboName.c_str(), sceneView->GetUBO());

        rg.ImportSwapChain("ForwardColor", swapchain);
        rg.AddImage("ForwardDepthStencil", rdg::GraphImage{{width, height, 1}, 1, 1, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED});

        auto forwardPass = rdg.AddRasterPass("forwardColor", width, height)
                         .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                         .AddAttachment({"ForwardDepthStencil", rhi::LoadOp::CLEAR, rhi::StoreOp::DONT_CARE}, rhi::ClearValue(1.f, 0));

        forwardPass.AddRasterSubPass("color0_sub0")
            .AddColor("ForwardColor", rdg::ResourceAccessBit::WRITE)
            .AddDepthStencil("ForwardDepthStencil", rdg::ResourceAccessBit::WRITE)
            .AddComputeView(uboName, {"ViewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS})
            .AddQueue("queue1")
                .SetRasterID("ForwardColor")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

        rdg.AddPresentPass("present", "ForwardColor");
    }

} // namespace sky
