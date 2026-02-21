//
// Created by blues on 2024/9/3.
//

#include <render/adaptor/pipeline/ForwardMSAAPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/light/LightFeatureProcessor.h>

namespace sky {

    ForwardMSAAPass::ForwardMSAAPass(rhi::PixelFormat format, rhi::PixelFormat ds, rhi::SampleCount samples_)
        : RasterPass(Name("ForwardMSAA"))
        , colorFormat(format)
        , depthStenFormat(ds)
        , samples(samples_)
    {
        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.format        = colorFormat;

        Name fwdMSAAColor(FWD_MSAA_CL.data());
        Name fwdColor(FWD_CL.data());
        Name fwdMSAADepthStencil(FWD_MSAA_DS.data());
        Name fwdDepthStencil(FWD_DS.data());

        if (samples_ > rhi::SampleCount::X1) {
            image.samples = samples;
            image.usage   = rhi::ImageUsageFlagBit::RENDER_TARGET;
            images.emplace_back(fwdMSAAColor, image);

            image.samples = rhi::SampleCount::X1;
            image.usage   = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
            images.emplace_back(fwdColor, image);

            image.samples = samples;
            image.usage   = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
            image.format  = depthStenFormat;
            images.emplace_back(fwdMSAADepthStencil, image);

            colors.emplace_back(
                Attachment{rdg::RasterAttachment{fwdMSAAColor, rhi::LoadOp::CLEAR, rhi::StoreOp::DONT_CARE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 0.f)});

            resolves.emplace_back(
                Attachment{rdg::RasterAttachment{fwdColor, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE}, rhi::ClearValue(0, 0, 0, 0)});

            depthStencil =
                Attachment{rdg::RasterAttachment{fwdMSAADepthStencil, rhi::LoadOp::CLEAR, rhi::StoreOp::DONT_CARE}, rhi::ClearValue(1.f, 0)};
        } else {
            image.samples = rhi::SampleCount::X1;
            image.usage   = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
            images.emplace_back(fwdColor, image);

            image.usage   = rhi::ImageUsageFlagBit::DEPTH_STENCIL | rhi::ImageUsageFlagBit::SAMPLED;
            image.format  = depthStenFormat;
            images.emplace_back(fwdDepthStencil, image);

            colors.emplace_back(
                Attachment{rdg::RasterAttachment{fwdColor, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(0.2f, 0.2f, 0.2f, 0.f)});

            depthStencil =
                Attachment{rdg::RasterAttachment{fwdDepthStencil, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE}, rhi::ClearValue(1.f, 0)};
        }

        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("ShadowMap"),
            rdg::ComputeView{Name("ShadowMap"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("BRDFLut"),
            rdg::ComputeView{Name("BRDFLut"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("IrradianceMap"),
            rdg::ComputeView{Name("IrradianceMap"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("PrefilteredMap"),
            rdg::ComputeView{Name("PrefilteredMap"), rdg::ComputeType::SRV, stageFlags}
        });

//        computeResources.emplace_back(ComputeResource{
//            Name("HizBuffer"),
//            rdg::ComputeView{Name("HizBuffer"), rdg::ComputeType::SRV, stageFlags}
//        });
    }

    void ForwardMSAAPass::SetLayout(const RDResourceLayoutPtr &layout_)
    {
        layout = layout_;
    }

    void ForwardMSAAPass::SetupSubPass(rdg::RasterSubPassBuilder& subPass, RenderScene &scene)
    {
        const Name viewName("MainCamera");

        subPass.AddQueue(Name("queue1"))
                .SetRasterID(Name("ForwardColor"))
                .SetSceneView(viewName)
                .SetLayout(layout);

        subPass.AddQueue(Name("queue2"))
                .SetRasterID(Name("Transparent"))
                .SetSceneView(viewName)
                .SetLayout(layout);

        subPass.AddQueue(Name("queue3"))
                .SetRasterID(Name("SkyBox"))
                .SetSceneView(viewName)
                .SetLayout(layout);
    }

} // namespace sky
