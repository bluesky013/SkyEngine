//
// Created by blues on 2024/9/6.
//

#include <render/adaptor/pipeline/ShadowMapPass.h>
#include <render/adaptor/Util.h>

#include <render/rdg/RenderGraph.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/Renderer.h>
#include <render/light/LightFeatureProcessor.h>

#include <core/math/Transform.h>

namespace sky {

    ShadowMapPass::ShadowMapPass(uint32_t width_, uint32_t height_)
        : RasterPass(Name("ShadowMap"))
        , shadowViewName("SHADOW_VIEW")
    {
        width  = width_;
        height = height_;

        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.usage         = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        image.format        = rhi::PixelFormat::D32;

        images.emplace_back(Name(SHADOW_MAP.data()), image);

        depthStencil = Attachment{
            rdg::RasterAttachment{Name(SHADOW_MAP.data()), rhi::LoadOp::CLEAR, rhi::StoreOp::STORE},
            rhi::ClearValue(1.f, 0)
        };

        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name(shadowViewName),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });
    }

    void ShadowMapPass::SetLayout(const RDResourceLayoutPtr &layout_)
    {
        layout = layout_;
    }

    void ShadowMapPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        if (isEnable) {
            RasterPass::Setup(rdg, scene);
        } else {
            auto tex = Renderer::Get()->GetDefaultResource().texture2DWhite;
            rdg.resourceGraph.ImportImage(Name("ShadowMap"), tex->GetImage(), rhi::ImageViewType::VIEW_2D, rhi::AccessFlagBit::FRAGMENT_SRV);
        }
    }

    void ShadowMapPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
        auto *lf = GetFeatureProcessor<LightFeatureProcessor>(&scene);
        auto* mainLight = lf->GetMainLight();

        if (sceneView == nullptr) {
            sceneView = scene.CreateSceneView(1);
        }
        if (mainLight != nullptr) {
            mainLight->BuildMatrix(*sceneView);
            sceneView->Update();
        }

        builder.rdg.resourceGraph.ImportUBO(shadowViewName, sceneView->GetUBO());
        builder.SetViewMask(0);

        builder.AddQueue(Name("queue1"))
            .SetRasterID(Name("Shadow"))
            .SetView(sceneView)
            .SetLayout(layout);
    }
} // namespace sky