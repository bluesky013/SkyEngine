//
// Created by blues on 2024/9/5.
//

#include <render/adaptor/pipeline/PostProcessingPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    PostProcessingPass::PostProcessingPass(const RDGfxTechPtr &tech)
        : FullScreenPass(Name("PostProcessingPass"), tech)
    {
        Name swapChainName(SWAP_CHAIN.data());
        Name fwdColor(FWD_CL.data());

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{swapChainName, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });

        computeResources.emplace_back(ComputeResource{
            fwdColor,
            rdg::ComputeView{"InColor", rdg::ComputeType::SRV, rhi::ShaderStageFlagBit::FS}
        });
    }

    void PostProcessingPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        FullScreenPass::Setup(rdg, scene);
    }

    void PostProcessingPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
        builder.AddQueue(Name("queue")).SetRasterID(Name("ui"));
    }

} // namespace sky