//
// Scene pass: HDR color + depth attachments, single "opaque" queue (front-to-back).
//

#include <aurora/pipeline/ScenePass.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/Device.h>

namespace sky::aurora {

    namespace {
        std::vector<RgBlockDesc> MakeScenePassBlocks()
        {
            // pass tier (set 1): per-pass params cbuffer
            RgBlockDesc block{};
            block.set       = 1;
            block.binding   = 0;
            block.blockName = Name("ScenePassParams");
            block.kind      = RgBlockKind::CBUFFER;
            block.fields    = {
                {RgFieldType::FLOAT4, Name("Misc")}, // placeholder params
            };
            return {block};
        }
    } // namespace

    ScenePass::ScenePass() : SceneRasterPassTemplate(Name("ScenePass"))
    {
    }

    const std::vector<RgBlockDesc> &ScenePass::GetPassBlocks() const
    {
        static const std::vector<RgBlockDesc> blocks = MakeScenePassBlocks();
        return blocks;
    }

    void ScenePass::OnSetup(Device *device)
    {
        SceneRasterPassTemplate::OnSetup(device);
        // TODO: create persistent PSO once shader pipeline is wired in
    }

    void ScenePass::OnSceneChanged()
    {
        SceneRasterPassTemplate::OnSceneChanged();
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
        graph.AddSceneRasterPass(mName,
            [this](SceneRasterPassBuilder &builder) {
                builder.ColorAttachment(0, mHdrColor, LoadOp::CLEAR, StoreOp::STORE);
                builder.DepthStencilAttachment(mDepth, LoadOp::CLEAR, StoreOp::DONT_CARE,
                                               LoadOp::DONT_CARE, StoreOp::DONT_CARE);

                if (GetPassResourceGroup() != nullptr) {
                    builder.SetPassResourceGroup(GetPassResourceGroup());
                }

                DeclareQueue(builder, Name("opaque"), Name("opaque"), QueueSortPolicy::FRONT_TO_BACK);

                Collect(builder);
            });

        graph.MarkOfInterest(mHdrColor);
    }

} // namespace sky::aurora
