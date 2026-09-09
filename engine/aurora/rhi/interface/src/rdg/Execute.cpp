//
// Aurora RDG execute phase (shared, backend-agnostic pass emission).
//

#include <aurora/rdg/RenderGraph.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/PipelineState.h>

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

            if (std::holds_alternative<SceneRasterPassTag>(pass.tag)) {
                const auto &data = mSceneRasterPasses[pass.payloadIndex];

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
                for (const auto &item : data.items) {
                    if (item.batchResourceGroup != nullptr) {
                        enc->BindResourceGroup(2, item.batchResourceGroup, 0, nullptr);
                    }
                    if (item.pso != nullptr) {
                        enc->BindPipeline(item.pso);
                    }
                    if (item.ib != nullptr) {
                        enc->BindIndexBuffer(item.ib, item.ibOffset, IndexType::U32);
                        enc->DrawIndexed(item.args);
                    }
                }
                enc->EndRendering();
            } else if (std::holds_alternative<FullScreenPassTag>(pass.tag)) {
                const auto &data = mFullScreenPasses[pass.payloadIndex];

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
                if (data.passResourceGroup != nullptr) {
                    enc->BindResourceGroup(1, data.passResourceGroup, 0, nullptr);
                }
                if (data.pso != nullptr) {
                    enc->BindPipeline(data.pso);
                    CmdDrawLinear draw{};
                    draw.vertexCount   = 3;
                    draw.firstVertex   = 0;
                    draw.instanceCount = 1;
                    draw.firstInstance = 0;
                    enc->Draw(draw);
                }
                enc->EndRendering();
            } else if (std::holds_alternative<ComputePassTag>(pass.tag)) {
                const auto &data = mComputePasses[pass.payloadIndex];
                auto enc = cmdBuf->CreateComputeEncoder();
                if (data.pso != nullptr) {
                    enc->BindPipeline(data.pso);
                }
                if (data.passResourceGroup != nullptr) {
                    enc->BindResourceGroup(1, data.passResourceGroup, 0, nullptr);
                }
                if (data.executeFn) {
                    data.executeFn(*enc, ctx);
                }
            } else if (std::holds_alternative<CopyBlitPassTag>(pass.tag)) {
                const auto &data = mCopyBlitPasses[pass.payloadIndex];
                auto enc = cmdBuf->CreateBlitEncoder();
                if (data.executeFn) {
                    data.executeFn(*enc, ctx);
                }
            } else if (std::holds_alternative<PresentPassTag>(pass.tag)) {
                // no extra encoder ops; barrier already emitted in frontBarriers
            } else if (std::holds_alternative<CustomPassTag>(pass.tag)) {
                const auto &data = mCustomPasses[pass.payloadIndex];
                if (data.fn) {
                    data.fn(ctx, *cmdBuf);
                }
            }
        }

        for (const BarrierInfo &barrier : mFinalBarriers) {
            cmdBuf->PipelineBarrier(barrier);
        }
    }

} // namespace sky::aurora
