//
// Scene pass: HDR color + depth attachments, single "opaque" queue (front-to-back).
//

#include <aurora/pipeline/ScenePass.h>

#include "ScenePass.h"
#include <aurora/rdg/RenderGraph.h>

namespace sky::aurora {

    ScenePass::ScenePass() : SceneRasterPassTemplate(Name("ScenePass"))
    {
    }

    const std::vector<RgBlockDesc> &ScenePass::GetPassBlocks() const
    {
        static const std::vector<RgBlockDesc> blocks{shared::GetScenePassParamsBlockDesc()};
        return blocks;
    }

    void ScenePass::BuildRDG(RenderGraph &graph)
    {
        if (mWidth == 0 || mHeight == 0) {
            return;
        }

        RDGTextureDesc colorDesc{};
        colorDesc.extent      = {mWidth, mHeight, 1};
        colorDesc.format      = PixelFormat::RGBA16_SFLOAT;
        colorDesc.mipLevels   = 1;
        colorDesc.arrayLayers = 1;
        colorDesc.samples     = SampleCount::X1;
        colorDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;

        RDGTextureDesc depthDesc{};
        depthDesc.extent      = {mWidth, mHeight, 1};
        depthDesc.format      = PixelFormat::D32;
        depthDesc.mipLevels   = 1;
        depthDesc.arrayLayers = 1;
        depthDesc.samples     = SampleCount::X1;
        depthDesc.usage       = ImageUsageFlagBit::DEPTH_STENCIL;

        mHdrColor = graph.CreateTexture(Name("SceneHDRColor"), colorDesc);
        mDepth    = graph.CreateTexture(Name("SceneDepth"), depthDesc);

        ResetQueues();
        graph.AddSceneRasterPass(mName, [this](SceneRasterPassBuilder &builder) {
            builder.ColorAttachment(0, mHdrColor, LoadOp::CLEAR, StoreOp::STORE);
            builder.DepthStencilAttachment(mDepth, LoadOp::CLEAR, StoreOp::DONT_CARE, LoadOp::DONT_CARE, StoreOp::DONT_CARE);

            if (GetPassResourceGroup() != nullptr) {
                builder.SetPassResourceGroup(GetPassResourceGroup());
            }

            DeclareQueue(builder, Name("opaque"), Name("opaque"), QueueSortPolicy::FRONT_TO_BACK);

            Collect(builder);
        });

        graph.MarkOfInterest(mHdrColor);
    }

} // namespace sky::aurora
