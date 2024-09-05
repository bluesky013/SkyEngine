//
// Created by blues on 2024/9/6.
//

#include <render/adaptor/pipeline/DefaultForwardPipeline.h>
#include <render/adaptor/assets/TechniqueAsset.h>
#include <render/rdg/RenderGraph.h>
#include <render/RenderWindow.h>
#include <render/RenderScene.h>
#include <render/RHI.h>
#include <framework/asset/AssetManager.h>

namespace sky {

    void DefaultForwardPipeline::InitPass()
    {
        auto postTechAsset = AssetManager::Get()->LoadAssetFromPath("techniques/post_processing.tech");
        postTechAsset->BlockUntilLoaded();
        auto postTech = GreateGfxTechFromAsset(std::static_pointer_cast<Asset<Technique>>(postTechAsset));

        forward     = std::make_unique<ForwardMSAAPass>(rhi::PixelFormat::RGBA16_SFLOAT, depthStencilFormat, rhi::SampleCount::X4);
        postProcess = std::make_unique<PostProcessingPass>(postTech);
        present     = std::make_unique<PresentPass>(output->GetSwapChain());
    }

    void DefaultForwardPipeline::SetOutput(RenderWindow *wnd)
    {
        output = wnd;
        if (!RHI::Get()->GetDevice()->CheckFormatFeature(depthStencilFormat, rhi::PixelFormatFeatureFlagBit::DEPTH_STENCIL)) {
            depthStencilFormat = rhi::PixelFormat::D32_S8;
        }

        globalUbo = std::make_shared<UniformBuffer>();
        globalUbo->Init(sizeof(ShaderPassInfo));

        InitPass();
    }

    void DefaultForwardPipeline::Collect(rdg::RenderGraph &rdg)
    {
        const auto renderWidth  = output->GetWidth();
        const auto renderHeight = output->GetHeight();

        auto *sceneView = scene->GetSceneViews()[0].get();

        const char* globalUboName = "globalUBO";
        const char* colorViewName = "SCENE_VIEW";

        auto &rg = rdg.resourceGraph;
        rg.ImportUBO(colorViewName, sceneView->GetUBO());
        rg.ImportUBO(globalUboName, globalUbo);

        forward->Resize(renderWidth, renderHeight);
        AddPass(forward.get());

        postProcess->Resize(renderWidth, renderHeight);
        AddPass(postProcess.get());

        AddPass(present.get());
    }

} // namespace sky
