//
// Created by Copilot on 2026/3/7.
//

#include <render/adaptor/pipeline/TSAAPass.h>
#include <render/adaptor/pipeline/DefaultPassConstants.h>
#include <render/RHI.h>
#include <render/RenderScene.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    TSAAPass::TSAAPass(const RDGfxTechPtr &tech, rhi::PixelFormat colorFormat)
        : FullScreenPass(Name("TSAAPass"), tech)
        , tsaaColorName(TSAA_CL.data())
        , tsaaHistoryName(TSAA_HIST.data())
        , format(colorFormat)
    {
        auto stageFlags = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS;

        colors.emplace_back(Attachment{
            rdg::RasterAttachment{tsaaColorName, rhi::LoadOp::DONT_CARE, rhi::StoreOp::STORE},
            rhi::ClearValue(0.f, 0.f, 0.f, 0.f)
        });

        computeResources.emplace_back(ComputeResource{
            Name(FWD_CL.data()),
            rdg::ComputeView{Name("InColor"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            tsaaHistoryName,
            rdg::ComputeView{Name("HistoryColor"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name(FWD_DS.data()),
            rdg::ComputeView{Name("DepthTex"), rdg::ComputeType::SRV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("SCENE_VIEW"),
            rdg::ComputeView{Name("viewInfo"), rdg::ComputeType::CBV, stageFlags}
        });

        computeResources.emplace_back(ComputeResource{
            Name("FWD_PassInfo"),
            rdg::ComputeView{Name("passInfo"), rdg::ComputeType::CBV, stageFlags}
        });
    }

    void TSAAPass::Setup(rdg::RenderGraph &rdg, RenderScene &scene)
    {
        auto &rsg = rdg.resourceGraph;
        uint32_t readIndex = 1 - writeIndex;

        // Create persistent history images on first frame
        if (firstFrame) {
            rhi::Image::Descriptor desc = {};
            desc.imageType   = rhi::ImageType::IMAGE_2D;
            desc.format      = format;
            desc.extent      = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
            desc.mipLevels   = 1;
            desc.arrayLayers = 1;
            desc.samples     = rhi::SampleCount::X1;
            desc.usage       = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
            desc.memory      = rhi::MemoryType::GPU_ONLY;

            historyImages[0] = RHI::Get()->GetDevice()->CreateImage(desc);
            historyImages[1] = RHI::Get()->GetDevice()->CreateImage(desc);
        }

        // Recreate if resolution changed
        if (!firstFrame && historyImages[0] &&
            (historyImages[0]->GetDescriptor().extent.width != width ||
             historyImages[0]->GetDescriptor().extent.height != height)) {
            rhi::Image::Descriptor desc = {};
            desc.imageType   = rhi::ImageType::IMAGE_2D;
            desc.format      = format;
            desc.extent      = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
            desc.mipLevels   = 1;
            desc.arrayLayers = 1;
            desc.samples     = rhi::SampleCount::X1;
            desc.usage       = rhi::ImageUsageFlagBit::RENDER_TARGET | rhi::ImageUsageFlagBit::SAMPLED;
            desc.memory      = rhi::MemoryType::GPU_ONLY;

            historyImages[0] = RHI::Get()->GetDevice()->CreateImage(desc);
            historyImages[1] = RHI::Get()->GetDevice()->CreateImage(desc);
            firstFrame = true;
        }

        // Import write target as render target (TSAAColor)
        rsg.ImportImage(tsaaColorName, historyImages[writeIndex], rhi::ImageViewType::VIEW_2D,
            firstFrame ? rhi::AccessFlagBit::NONE : rhi::AccessFlagBit::COLOR_WRITE);

        // Import read target as SRV (TSAAHistory)
        rsg.ImportImage(tsaaHistoryName, historyImages[readIndex], rhi::ImageViewType::VIEW_2D,
            firstFrame ? rhi::AccessFlagBit::NONE : rhi::AccessFlagBit::FRAGMENT_SRV);

        FullScreenPass::Setup(rdg, scene);

        // Swap ping-pong index for next frame
        writeIndex = readIndex;
        firstFrame = false;
    }

    void TSAAPass::SetupSubPass(rdg::RasterSubPassBuilder& builder, RenderScene &scene)
    {
    }

} // namespace sky
