//
// Created by blues on 2024/9/6.
//

#include <framework/asset/AssetManager.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/RenderWindow.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/adaptor/pipeline/DefaultForwardPipeline.h>
#include <render/adaptor/Util.h>
#include <render/light/LightFeatureProcessor.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    void DefaultForwardPipeline::InitPass()
    {
        auto postTechAsset = AssetManager::Get()->LoadAssetFromPath("techniques/post_processing.tech");
        postTechAsset->BlockUntilLoaded();
        auto postTech = GreateGfxTechFromAsset(std::static_pointer_cast<Asset<Technique>>(postTechAsset));

        auto brdfAsset = AssetManager::Get()->LoadAssetFromPath("techniques/brdf_lut.tech");
        brdfAsset->BlockUntilLoaded();
        auto brdfTech = GreateGfxTechFromAsset(std::static_pointer_cast<Asset<Technique>>(brdfAsset));

        rhi::DescriptorSetLayout::Descriptor desc = {};
        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        desc.bindings.emplace_back(
            rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, stageFlags, "passInfo");
        desc.bindings.emplace_back(
            rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, stageFlags, "viewInfo");
        desc.bindings.emplace_back(
                rhi::DescriptorType::SAMPLED_IMAGE, 1, 2, stageFlags, "ShadowMap");
        desc.bindings.emplace_back(
                rhi::DescriptorType::SAMPLER, 1, 3, stageFlags, "ShadowMapSampler");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLED_IMAGE, 1, 4, stageFlags, "BRDFLut");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLER, 1, 5, stageFlags, "BRDFLutSampler");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLED_IMAGE, 1, 6, stageFlags, "IrradianceMap");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLER, 1, 7, stageFlags, "IrradianceSampler");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLED_IMAGE, 1, 8, stageFlags, "PrefilteredMap");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLER, 1, 9, stageFlags, "PrefilteredMapSampler");


        defaultRasterLayout = new ResourceGroupLayout();
        defaultRasterLayout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        defaultRasterLayout->AddNameHandler(Name("passInfo"), {0, sizeof(ShaderPassInfo)});
        defaultRasterLayout->AddNameHandler(Name("viewInfo"), {1, sizeof(SceneViewInfo)});
        defaultRasterLayout->AddNameHandler(Name("ShadowMap"), {2});
        defaultRasterLayout->AddNameHandler(Name("ShadowMapSampler"), {3});
        defaultRasterLayout->AddNameHandler(Name("BRDFLut"), {4});
        defaultRasterLayout->AddNameHandler(Name("BRDFLutSampler"), {5});
        defaultRasterLayout->AddNameHandler(Name("IrradianceMap"), {6});
        defaultRasterLayout->AddNameHandler(Name("IrradianceSampler"), {7});
        defaultRasterLayout->AddNameHandler(Name("PrefilteredMap"), {8});
        defaultRasterLayout->AddNameHandler(Name("PrefilteredMapSampler"), {9});

        defaultGlobal = new UniformBuffer();
        defaultGlobal->Init(sizeof(ShaderPassInfo));

        depth = std::make_unique<DepthPass>(depthStencilFormat, rhi::SampleCount::X1);
        depth->SetLayout(defaultRasterLayout);

        forward = std::make_unique<ForwardMSAAPass>(rhi::PixelFormat::RGBA16_SFLOAT, depthStencilFormat, rhi::SampleCount::X4);
        forward->SetLayout(defaultRasterLayout);

        shadowMap = std::make_unique<ShadowMapPass>(4096, 4096);
        shadowMap->SetLayout(defaultRasterLayout);

        brdfLut     = std::make_unique<BRDFLutPass>(brdfTech);
        postProcess = std::make_unique<PostProcessingPass>(postTech);
        present     = std::make_unique<PresentPass>(output->GetSwapChain());
    }

    void DefaultForwardPipeline::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
        if (!RHI::Get()->GetDevice()->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
            depthStencilFormat = rhi::PixelFormat::D32_S8;
        }

        InitPass();
    }

    void DefaultForwardPipeline::SetupGlobal(rdg::RenderGraph &rdg)
    {
        ShaderPassInfo info = {};
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(scene);
        if (lf != nullptr) {
            auto *mainLight = lf ->GetMainLight();
            if (mainLight != nullptr) {
                info.lightMatrix = mainLight->GetMatrix();
                info.mainLightColor = mainLight->GetColor();
                info.mainLightDirection = mainLight->GetDirection();
            }
        }
        defaultGlobal->WriteT(0, info);

        Name colorViewName = Name("SCENE_VIEW");
        Name fwdPassInfoName = Name("FWD_PassInfo");
        Name mainCameraName = Name("MainCamera");

        auto *sceneView = scene->GetSceneView(mainCameraName);
        auto &rg = rdg.resourceGraph;
        rg.ImportUBO(colorViewName, sceneView->GetUBO());
        rg.ImportUBO(fwdPassInfoName, defaultGlobal);
    }

    void DefaultForwardPipeline::Collect(rdg::RenderGraph &rdg)
    {
        const auto renderWidth  = output->GetWidth();
        const auto renderHeight = output->GetHeight();

        SetupGlobal(rdg);

        shadowMap->SetEnable(true);

        AddPass(brdfLut.get());
        AddPass(shadowMap.get());

//        depth->Resize(renderWidth, renderHeight);
//        AddPass(depth.get());

        forward->Resize(renderWidth, renderHeight);
        AddPass(forward.get());

        postProcess->Resize(renderWidth, renderHeight);
        AddPass(postProcess.get());

        AddPass(present.get());
    }

} // namespace sky
