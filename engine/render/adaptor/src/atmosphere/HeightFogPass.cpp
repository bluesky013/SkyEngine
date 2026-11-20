//
// Created by SkyEngine on 2025/3/7.
//

#include <render/adaptor/atmosphere/HeightFogPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    HeightFogPass::HeightFogPass(const RDGfxTechPtr &tech)
        : FullScreenPass(Name("HeightFogPass"), tech)
    {
        Name fogOutputName(HEIGHT_FOG_OUTPUT.data());
        Name fwdColor(FWD_CL.data());
        Name fwdDepth(FWD_DS.data());

        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.format        = rhi::PixelFormat::RGBA16_SFLOAT;
        image.usage         = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
        image.samples       = rhi::SampleCount::X1;
        images.emplace_back(fogOutputName, image);

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{fogOutputName, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0.f, 0.f, 0.f, 0.f)
        });

        auto stageFlags = rhi::ShaderStageFlagBit::FS;

        // View info (contains InvViewProj for world-space reconstruction)
        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        // Height fog parameters UBO
        computeResources.emplace_back(ComputeResource{
            Name("HeightFogParamsBuffer"),
            rdg::ComputeView{Name("heightFogCB"), rdg::ComputeType::CBV, stageFlags}
        });

        // Scene color input
        computeResources.emplace_back(ComputeResource{
            fwdColor,
            rdg::ComputeView{Name("InColor"), rdg::ComputeType::SRV, stageFlags}
        });

        // Scene depth input
        computeResources.emplace_back(ComputeResource{
            fwdDepth,
            rdg::ComputeView{Name("InDepth"), rdg::ComputeType::SRV, stageFlags}
        });

        // Samplers
        samplers.emplace_back(SamplerResource{Name("PointSampler"), Name("InColorSampler")});
        samplers.emplace_back(SamplerResource{Name("PointSampler"), Name("InDepthSampler")});
    }

    void HeightFogPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        FullScreenPass::Setup(rdg, scene);
    }

} // namespace sky
