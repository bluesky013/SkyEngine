//
// Created by Zach Lee on 2023/8/20.
//

#include <render/RenderScene.h>
#include <render/RenderWindow.h>
#include <render/rdg/RenderGraph.h>
#include <render/adaptor/pipeline/DefaultForward.h>
#include <render/adaptor/assets/TechniqueAsset.h>

#include <core/math/Transform.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void DefaultForward::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
        if (!rdgContext->device->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
            depthStencilFormat = rhi::PixelFormat::D32_S8;
        }

        if (!postTech) {
            auto tech = AssetManager::Get()->LoadAssetFromPath("techniques/post_processing.tech");
            tech->BlockUntilLoaded();

            postTech = tech ? GreateGfxTechFromAsset(std::static_pointer_cast<Asset<Technique>>(tech)) : nullptr;
        }

        globalUbo = std::make_shared<UniformBuffer>();
        globalUbo->Init(sizeof(ShaderPassInfo));

        {
            rhi::DescriptorSetLayout::Descriptor desc = {};
            desc.bindings.emplace_back(
                rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::FS, "passInfo");
            desc.bindings.emplace_back(
                rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo");
            desc.bindings.emplace_back(
                rhi::DescriptorType::SAMPLED_IMAGE, 1, 2, rhi::ShaderStageFlagBit::FS, "ShadowMap");
            desc.bindings.emplace_back(
                rhi::DescriptorType::SAMPLER, 1, 3, rhi::ShaderStageFlagBit::FS, "ShadowMapSampler");

            forwardLayout = new ResourceGroupLayout();
            forwardLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
            forwardLayout->AddNameHandler("passInfo", {0, sizeof(ShaderPassInfo)});
            forwardLayout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
            forwardLayout->AddNameHandler("ShadowMap", {2});
            forwardLayout->AddNameHandler("ShadowMapSampler", {3});
        }

        {
            rhi::DescriptorSetLayout::Descriptor desc = {};
            desc.bindings.emplace_back(rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS, "viewInfo");
            shadowLayout = new ResourceGroupLayout();
            shadowLayout->SetRHILayout(rdgContext->device->CreateDescriptorSetLayout(desc));
            shadowLayout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
        }
    }

    bool DefaultForward::OnSetup(rdg::RenderGraph &rdg, const std::vector<RenderScene*> &scenes)
    {
        const auto &swapChain = output->GetSwaChain();
        const auto &ext = swapChain->GetExtent();
        const auto renderWidth  = ext.width;
        const auto renderHeight = ext.height;

        auto &rg = rdg.resourceGraph;

        const auto &views = scenes[0]->GetSceneViews();
        if (views.empty()) {
            return false;
        }

        if (shadowScene == nullptr) {
            Transform trans = {};
            trans.translation = Vector3(21, 28, -7);
            trans.rotation.FromEulerYZX(Vector3(-50, 113, 0));

            shadowScene = scenes[0]->CreateSceneView(1);
            shadowScene->SetPerspective(1.f, 100.f, 75.f / 180.f * PI, 1.f);
            shadowScene->SetMatrix(trans.ToMatrix());
            shadowScene->Update();
        }

        auto *sceneView = views[0].get();

        const char* globalUboName = "globalUBO";
        const char* colorViewName = "SCENE_VIEW";
        const char* shadowViewName = "SHADOW_VIEW";

        rg.ImportSwapChain("SwapChain", swapChain);
        rg.ImportUBO(colorViewName, sceneView->GetUBO());
        rg.ImportUBO(shadowViewName, shadowScene->GetUBO());
        rg.ImportUBO(globalUboName, globalUbo);

        rg.AddImage("ShadowMap", rdg::GraphImage{{4096, 4096, 1}, 1, 1, rhi::PixelFormat::D32, rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X1, rhi::ImageViewType::VIEW_2D});
        rdg.AddRasterPass("ShadowPass", 4096, 4096)
                .AddAttachment({"ShadowMap", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0))
                .AddRasterSubPass("ShadowSub")
                .AddDepthStencil("ShadowMap", rdg::ResourceAccessBit::WRITE);
//                .SetViewMask(viewMask)
//                .AddComputeView(shadowViewName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS})
//                .AddComputeView(globalUboName, {"passInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS})
//                .AddQueue("queue1")
//                .SetRasterID("DepthOnly")
//                .SetLayout(shadowLayout);

        rhi::PixelFormat hdrFormat = rhi::PixelFormat::RGBA16_SFLOAT;
        auto viewType = rhi::ImageViewType::VIEW_2D;

        rg.AddImage("ForwardColor", rdg::GraphImage{{renderWidth, renderHeight, 1}, 1, 1, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED, rhi::SampleCount::X1, viewType});
        rg.AddImage("ForwardColorMSAA", rdg::GraphImage{{renderWidth, renderHeight, 1}, 1, 1, hdrFormat, rhi::ImageUsageFlagBit::RENDER_TARGET, rhi::SampleCount::X2, viewType});
        rg.AddImage("ForwardDSMSAA", rdg::GraphImage{{renderWidth, renderHeight, 1}, 1, 1, depthStencilFormat, rhi::ImageUsageFlagBit::DEPTH_STENCIL, rhi::SampleCount::X2, viewType});

        auto forwardPass = rdg.AddRasterPass("forwardColor", renderWidth, renderHeight)
                .AddAttachment({"ForwardColor", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                .AddAttachment({"ForwardColorMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 1.f))
                .AddAttachment({"ForwardDSMSAA", rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0));

        auto subpass = forwardPass.AddRasterSubPass("color0_sub0");
        subpass.AddColor("ForwardColorMSAA", rdg::ResourceAccessBit::WRITE)
                .AddDepthStencil("ForwardDSMSAA", rdg::ResourceAccessBit::WRITE)
                .AddResolve("ForwardColor", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("ShadowMap", {"ShadowMap", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS})
                .AddComputeView(colorViewName, {"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS})
                .AddComputeView(globalUboName, {"passInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS});
        subpass.SetViewMask(viewMask);

        subpass.AddQueue("queue1")
                .SetRasterID("ForwardColor")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

        subpass.AddQueue("queue2")
                .SetRasterID("SkyBox")
                .SetView(sceneView)
                .SetLayout(forwardLayout);

        auto pp = rdg.AddRasterPass("PostProcessing", renderWidth, renderHeight)
                .AddAttachment({"SwapChain", rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, {});
        auto ppSub = pp.AddRasterSubPass("pp_sub0");
        ppSub.AddColor("SwapChain", rdg::ResourceAccessBit::WRITE)
                .AddComputeView("ForwardColor", {"InColor", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS});
        ppSub.AddFullScreen("fullscreen")
                .SetTechnique(postTech);

        ppSub.AddQueue("queue").SetRasterID("ui");
        rdg.AddPresentPass("present", "SwapChain");

        return true;
    }

} // namespace sky
