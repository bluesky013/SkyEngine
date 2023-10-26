//
// Created by Zach Lee on 2023/8/20.
//

#include "render/RenderScene.h"
#include "render/RenderWindow.h"
#include "render/rdg/RenderGraph.h"
#include <render/adaptor/pipeline/DefaultForward.h>

namespace sky {

    void DefaultForward::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
        if (!rdgContext->device->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
            depthStencilFormat = rhi::PixelFormat::D32_S8;
        }

        rhi::DescriptorSetLayout::Descriptor desc = {};
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "passInfo"});
        desc.bindings.emplace_back(rhi::DescriptorSetLayout::SetBinding {rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo"});

        forwardLayout = std::make_shared<ResourceGroupLayout>();
        forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
        forwardLayout->AddNameHandler("passInfo", {0, sizeof(ShaderPassInfo)});
        forwardLayout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
    }

    void DefaultForward::OnSetup(rdg::RenderGraph &rdg)
    {
        const auto &swapchain = output->GetSwaChain();
        const auto &ext = swapchain->GetExtent();
        const auto width  = ext.width;
        const auto height = ext.height;

        auto &rg = rdg.resourceGraph;

        const auto &views = rdg.scene->GetSceneViews();
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

        auto sub = forwardPass.AddRasterSubPass("color0_sub0")
            .AddColor("ForwardColor", rdg::ResourceAccessBit::WRITE)
            .AddDepthStencil("ForwardDepthStencil", rdg::ResourceAccessBit::WRITE)
            .AddComputeView(uboName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS});

        sub.AddQueue("queue1")
            .SetRasterID("ForwardColor")
            .SetView(sceneView)
            .SetLayout(forwardLayout);

        sub.AddQueue("ui")
            .SetRasterID("ui");

        rdg.AddPresentPass("present", "ForwardColor");
    }

} // namespace sky
