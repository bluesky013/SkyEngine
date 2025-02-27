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
#include <rhi/Util.h>

namespace sky {

    RDTechniquePtr LoadGfxTech(const std::string& name)
    {
        auto asset = AssetManager::Get()->LoadAssetFromPath(name);
        asset->BlockUntilLoaded();
        return CreateGfxTechFromAsset(std::static_pointer_cast<Asset<Technique>>(asset));
    }

    void DefaultForwardPipeline::InitPass()
    {
        auto postTech = LoadGfxTech("techniques/post_processing.tech");
        auto brdfTech = LoadGfxTech("techniques/brdf_lut.tech");
        auto depthResolveTech = LoadGfxTech("techniques/depth_resolve.tech");
        auto depthDownSampleTech = LoadGfxTech("techniques/depth_downsample.tech");

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
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLED_IMAGE, 1, 10, stageFlags, "HizBuffer");
        desc.bindings.emplace_back(
            rhi::DescriptorType::SAMPLER, 1, 11, stageFlags, "HizBufferSampler");

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
        defaultRasterLayout->AddNameHandler(Name("HizBuffer"), {10});
        defaultRasterLayout->AddNameHandler(Name("HizBufferSampler"), {11});

        defaultGlobal = new UniformBuffer();
        defaultGlobal->Init(sizeof(ShaderPassInfo));

        depth = std::make_unique<DepthPass>(depthStencilFormat, rhi::SampleCount::X1);
        depth->SetLayout(defaultRasterLayout);

        forward = std::make_unique<ForwardMSAAPass>(rhi::PixelFormat::RGBA16_SFLOAT, depthStencilFormat, rhi::SampleCount::X1);
        forward->SetLayout(defaultRasterLayout);

        shadowMap = std::make_unique<ShadowMapPass>(4096, 4096);
        shadowMap->SetLayout(defaultRasterLayout);

        brdfLut     = std::make_unique<BRDFLutPass>(brdfTech);
        postProcess = std::make_unique<PostProcessingPass>(postTech);
        present     = std::make_unique<PresentPass>(output->GetSwapChain());

        hiz = std::make_unique<HizGenerator>(depthResolveTech, depthDownSampleTech);

        rhi::Sampler::Descriptor samplerDesc = {};
        samplerDesc.minFilter = rhi::Filter::NEAREST;
        samplerDesc.magFilter = rhi::Filter::NEAREST;
        pointSampler = RHI::Get()->GetDevice()->CreateSampler(samplerDesc);
    }

    void DefaultForwardPipeline::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
//        if (!RHI::Get()->GetDevice()->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
//            depthStencilFormat = rhi::PixelFormat::D32_S8;
//        }

        InitPass();
    }

    void DefaultForwardPipeline::SetupScreenExternalImages(rdg::RenderGraph &rdg, uint32_t w, uint32_t h)
    {
        bool first = false;
        if (!hizDepth || hizDepth->GetDescriptor().extent.width != w || hizDepth->GetDescriptor().extent.height != h) {
            rhi::Image::Descriptor desc = {};
            desc.imageType   = rhi::ImageType::IMAGE_2D;
            desc.format      = rhi::PixelFormat::R32_SFLOAT;
            desc.extent      = {w, h, 1};
            desc.mipLevels   = rhi::GetMipLevel(w, h);
            desc.arrayLayers = 1;
            desc.samples     = rhi::SampleCount::X1;
            desc.usage       = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
            desc.memory      = rhi::MemoryType::GPU_ONLY;

            hizDepth = RHI::Get()->GetDevice()->CreateImage(desc);
            first = true;
        }

//        rdg.resourceGraph.ImportImage(Name("HizDepth"), hizDepth, rhi::ImageViewType::VIEW_2D_ARRAY,
//            first ? rhi::AccessFlagBit::NONE : rhi::AccessFlagBit::COLOR_WRITE);
    }

    void DefaultForwardPipeline::SetupGlobal(rdg::RenderGraph &rdg, uint32_t w, uint32_t h)
    {
        ShaderPassInfo info = {};
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(scene);
        if (lf != nullptr) {
            auto *mainLight = lf ->GetMainLight();
            if (mainLight != nullptr) {
                info.lightMatrix = mainLight->GetMatrix();
                info.mainLightColor = mainLight->GetColor();
                info.mainLightDirection = mainLight->GetDirection();
                info.viewport.z = (float)w;
                info.viewport.w = (float)h;
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

        rg.ImportSampler(Name("PointSampler"), pointSampler);
    }

    void DefaultForwardPipeline::Collect(rdg::RenderGraph &rdg)
    {
        const auto renderWidth  = output->GetWidth();
        const auto renderHeight = output->GetHeight();

        SetupGlobal(rdg, renderWidth, renderHeight);
        SetupScreenExternalImages(rdg, renderWidth, renderHeight);

        shadowMap->SetEnable(false);

        AddPass(brdfLut.get());
        AddPass(shadowMap.get());

//        depth->Resize(renderWidth, renderHeight);
//        AddPass(depth.get());

        forward->Resize(renderWidth, renderHeight);
        AddPass(forward.get());

        postProcess->Resize(renderWidth, renderHeight);
        AddPass(postProcess.get());

        hiz->BuildHizPass(rdg, hizDepth, renderWidth, renderHeight);
        hiz->AddPass(*this);

        AddPass(present.get());
    }

} // namespace sky
