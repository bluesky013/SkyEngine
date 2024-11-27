//
// Created by blues on 2024/9/3.
//

#include <render/adaptor/pipeline/ForwardMSAAPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>
#include <render/RHI.h>
#include <render/RenderScene.h>

namespace sky {

    ForwardMSAAPass::ForwardMSAAPass(rhi::PixelFormat format, rhi::PixelFormat ds, rhi::SampleCount samples_)
        : RasterPass(Name("ForwardMSAA"))
        , colorFormat(format)
        , depthStenFormat(rhi::PixelFormat::D24_S8)
        , samples(samples_)
    {
        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.samples       = samples;
        image.usage         = rhi::ImageUsageFlagBit::RENDER_TARGET;
        image.format        = colorFormat;

        Name fwdMSAAColor(FWD_MSAA_CL.data());
        Name fwdColor(FWD_CL.data());
        Name fwdMSAADepthStencil(FWD_MSAA_DS.data());

        images.emplace_back(fwdMSAAColor, image);

        image.samples       = rhi::SampleCount::X1;
        image.usage         = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
        images.emplace_back(fwdColor, image);

        image.samples       = samples;
        image.usage         = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        image.format        = depthStenFormat;
        images.emplace_back(fwdMSAADepthStencil, image);

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{fwdMSAAColor, rhi::LoadOp::CLEAR, rhi::StoreOp::DONT_CARE},
            rhi::ClearValue(0.2f, 0.2f, 0.2f, 0.f)
        });

        resolves.emplace_back(Attachment{
            rdg::RasterAttachment{fwdColor, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        depthStencil = Attachment{
            rdg::RasterAttachment{fwdMSAADepthStencil, rhi::LoadOp::CLEAR, rhi::StoreOp::DONT_CARE},
            rhi::ClearValue(1.f, 0)
        };

//        computeResources.emplace_back(ComputeResource{
//                FWD_CL.data(),
//                rdg::ComputeView{"globalUBO", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS}
//        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{"viewInfo", rdg::ComputeType::CBV, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS}
        });

        rhi::DescriptorSetLayout::Descriptor desc = {};
//        desc.bindings.emplace_back(
//                rhi::DescriptorType::UNIFORM_BUFFER, 1, 0, rhi::ShaderStageFlagBit::FS, "passInfo");
        desc.bindings.emplace_back(
                rhi::DescriptorType::UNIFORM_BUFFER, 1, 1, rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS, "viewInfo");
//        desc.bindings.emplace_back(
//                rhi::DescriptorType::SAMPLED_IMAGE, 1, 2, rhi::ShaderStageFlagBit::FS, "ShadowMap");
//        desc.bindings.emplace_back(
//                rhi::DescriptorType::SAMPLER, 1, 3, rhi::ShaderStageFlagBit::FS, "ShadowMapSampler");

        layout = new ResourceGroupLayout();
        layout->SetRHILayout(RHI::Get()->GetDevice()->CreateDescriptorSetLayout(desc));
        layout->AddNameHandler("passInfo", {0, sizeof(ShaderPassInfo)});
        layout->AddNameHandler("viewInfo", {1, sizeof(SceneViewInfo)});
//        layout->AddNameHandler("ShadowMap", {2});
//        layout->AddNameHandler("ShadowMapSampler", {3});
    }

    void ForwardMSAAPass::SetupSubPass(rdg::RasterSubPassBuilder& subPass, RenderScene &scene)
    {
        auto *sceneView = scene.GetSceneViews()[0].get();

        subPass.SetViewMask(0);

        subPass.AddQueue(Name("queue1"))
                .SetRasterID(Name("ForwardColor"))
                .SetView(sceneView)
                .SetLayout(layout);

        subPass.AddQueue(Name("queue2"))
                .SetRasterID(Name("Transparent"))
                .SetView(sceneView)
                .SetLayout(layout);

        subPass.AddQueue(Name("queue3"))
                .SetRasterID(Name("SkyBox"))
                .SetView(sceneView)
                .SetLayout(layout);
    }

} // namespace sky