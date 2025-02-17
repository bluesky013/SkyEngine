//
// Created by blues on 2025/2/3.
//

#include <render/adaptor/pipeline/DepthPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>
#include <render/RHI.h>
#include <render/RenderScene.h>

namespace sky {

    DepthPass::DepthPass(rhi::PixelFormat ds, rhi::SampleCount samples_)
        : RasterPass(Name("DepthPass"))
    {
        rdg::GraphImage image = {};
        image.extent.width  = width;
        image.extent.height = height;
        image.samples       = samples_;
        image.usage         = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        image.format        = ds;

        Name fwdDepthResolve(FWD_DS_RESOLVE.data());

        images.emplace_back(fwdDepthResolve, image);

        depthStencil = Attachment{
                    rdg::RasterAttachment{fwdDepthResolve, rhi::LoadOp::CLEAR, rhi::StoreOp::STORE},
            rhi::ClearValue(1.f, 0)
        };

        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS | rhi::ShaderStageFlagBit::TAS | rhi::ShaderStageFlagBit::MS;
        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });
    }

    void DepthPass::SetLayout(const RDResourceLayoutPtr &layout_)
    {
        layout = layout_;
    }

    void DepthPass::SetupSubPass(rdg::RasterSubPassBuilder& subPass, RenderScene &scene)
    {
        auto *sceneView = scene.GetSceneView(Name("MainCamera"));

        subPass.SetViewMask(0);

        subPass.AddQueue(Name("queue1"))
            .SetRasterID(Name("DepthOnly"))
            .SetView(sceneView)
            .SetLayout(layout);
    }

} // namespace sky