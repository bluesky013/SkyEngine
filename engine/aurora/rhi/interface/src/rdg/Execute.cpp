//
// Aurora RDG execute phase (shared, backend-agnostic pass emission).
//

#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>

namespace sky::aurora {

    void RenderGraph::Execute(CommandBuffer *cmdBuf)
    {
        if (!mCompiled || cmdBuf == nullptr) {
            return;
        }
        mBackend->Execute(*this, cmdBuf);
    }

    void RenderGraph::ExecutePasses(CommandBuffer *cmdBuf)
    {
        RDGContext ctx;
        ctx.SetCommandBuffer(cmdBuf);
        ctx.SetImageTable(&mResolvedImages);
        ctx.SetBufferTable(&mResolvedBuffers);

        for (const uint32_t passIndex : mTopoOrder) {
            const PassNode &pass = mPasses[passIndex];
            if (!pass.live) {
                continue;
            }

            ctx.SetPassName(pass.name);

            for (const BarrierInfo &barrier : pass.frontBarriers) {
                cmdBuf->PipelineBarrier(barrier);
            }

            if (std::holds_alternative<RasterPassTag>(pass.tag)) {
                const RasterPassData &data = mRasterPasses[pass.payloadIndex];

                RenderingInfo info{};
                info.renderArea = {{0, 0}, data.renderArea};
                info.numColors  = static_cast<uint32_t>(data.colors.size());
                for (size_t i = 0; i < data.colors.size() && i < MAX_COLOR_ATTACHMENTS; ++i) {
                    const auto &color          = data.colors[i];
                    info.colors[i].image       = mResolvedImages[color.resourceIndex];
                    info.colors[i].loadOp      = color.loadOp;
                    info.colors[i].storeOp     = color.storeOp;
                    info.colors[i].clearValue  = color.clearValue;
                }
                if (data.depthStencilResource != INVALID_INDEX) {
                    info.depthStencil.image          = mResolvedImages[data.depthStencilResource];
                    info.depthStencil.depthLoadOp    = data.depthLoadOp;
                    info.depthStencil.depthStoreOp   = data.depthStoreOp;
                    info.depthStencil.stencilLoadOp  = data.stencilLoadOp;
                    info.depthStencil.stencilStoreOp = data.stencilStoreOp;
                    info.depthStencil.clearValue     = data.depthStencilClear;
                }

                auto enc = cmdBuf->CreateGraphicsEncoder();
                enc->BeginRendering(info);
                if (data.executeFn) {
                    data.executeFn(*enc, ctx);
                }
                enc->EndRendering();
            } else if (std::holds_alternative<ComputePassTag>(pass.tag)) {
                const ComputePassData &data = mComputePasses[pass.payloadIndex];
                auto enc = cmdBuf->CreateComputeEncoder();
                if (data.executeFn) {
                    data.executeFn(*enc, ctx);
                }
            } else if (std::holds_alternative<CopyPassTag>(pass.tag)) {
                const CopyPassData &data = mCopyPasses[pass.payloadIndex];
                auto enc = cmdBuf->CreateBlitEncoder();
                if (data.executeFn) {
                    data.executeFn(*enc, ctx);
                }
            }
        }

        for (const BarrierInfo &barrier : mFinalBarriers) {
            cmdBuf->PipelineBarrier(barrier);
        }
    }

} // namespace sky::aurora
