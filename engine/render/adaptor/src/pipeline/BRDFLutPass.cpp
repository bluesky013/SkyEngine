//
// Created by blues on 2025/2/17.
//

#include <render/adaptor/pipeline/BRDFLutPass.h>
#include <render/RHI.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    BRDFLutPass::BRDFLutPass(const RDGfxTechPtr &tech)
        : FullScreenPass(Name("BRDFLut"), tech)
        , brdfTextureName("BRDFLut")
    {
        width = 512;
        height = 512;

        rhi::Image::Descriptor desc = {};
        desc.extent.width = width;
        desc.extent.height = height;
        desc.format = rhi::PixelFormat::RG32_SFLOAT;
        desc.usage = rhi::ImageUsageFlagBit::SAMPLED | rhi::ImageUsageFlagBit::RENDER_TARGET;

        brdfTexture = RHI::Get()->GetDevice()->CreateImage(desc);

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{brdfTextureName, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0, 0, 0, 0)
        });
    }

    void BRDFLutPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        if (firstTime) {
            rdg.resourceGraph.ImportImage(brdfTextureName, brdfTexture, rhi::ImageViewType::VIEW_2D, rhi::AccessFlagBit::NONE);

            FullScreenPass::Setup(rdg, scene);
            firstTime = false;
        } else {
            rdg.resourceGraph.ImportImage(brdfTextureName, brdfTexture, rhi::ImageViewType::VIEW_2D, rhi::AccessFlagBit::FRAGMENT_SRV);
        }
    }

    void BRDFLutPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
    }

} // namespace sky