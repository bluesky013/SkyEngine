//
// Opaque pass: color + depth attachments, single "opaque" queue (front-to-back).
//

#include <aurora/pipeline/OpaquePass.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/Device.h>

namespace sky::aurora {

    OpaquePass::OpaquePass() : SceneRasterPassTemplate(Name("OpaquePass"))
    {
    }

    void OpaquePass::OnSetup(Device *device)
    {
        (void)device;
        // TODO: create persistent PSO / ResourceGroup once shader pipeline is wired in
    }

    void OpaquePass::OnSceneChanged()
    {
        // TODO: rebuild persistent ResourceGroup when scene bindings change
    }

    void OpaquePass::BuildRDG(RenderGraph &graph)
    {
        if (mWidth == 0 || mHeight == 0) {
            return;
        }

        RDGTextureDesc colorDesc{};
        colorDesc.extent      = {mWidth, mHeight, 1};
        colorDesc.format      = PixelFormat::RGBA8_UNORM;
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

        mColor = graph.CreateTexture(Name("OpaqueColor"), colorDesc);
        mDepth = graph.CreateTexture(Name("OpaqueDepth"), depthDesc);

        graph.AddSceneRasterPass(mName,
            [this](SceneRasterPassBuilder &builder) {
                builder.ColorAttachment(0, mColor, LoadOp::CLEAR, StoreOp::STORE);
                builder.DepthStencilAttachment(mDepth, LoadOp::CLEAR, StoreOp::DONT_CARE,
                                               LoadOp::DONT_CARE, StoreOp::DONT_CARE);

                const uint32_t opaqueQueue = DeclareQueue(builder, Name("opaque"), QueueSortPolicy::FRONT_TO_BACK);
                (void)opaqueQueue;

                Collect(builder);
            });

        graph.MarkOfInterest(mColor);
    }

} // namespace sky::aurora
