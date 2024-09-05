//
// Created by blues on 2024/9/5.
//

#include <render/adaptor/pipeline/PostProcessingPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    PostProcessingPass::PostProcessingPass(const RDGfxTechPtr &tech)
        : FullScreenPass("PostProcessingPass", tech)
    {
        colors.emplace_back(Attachment{
            rdg::RasterAttachment{SWAP_CHAIN.data(), rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        computeResources.emplace_back(ComputeResource{
            FWD_CL.data(),
            rdg::ComputeView{"InColor", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS}
        });
    }

    void PostProcessingPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        FullScreenPass::Setup(rdg, scene);
    }

    void PostProcessingPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
        builder.AddQueue("queue").SetRasterID("ui");
    }

} // namespace sky