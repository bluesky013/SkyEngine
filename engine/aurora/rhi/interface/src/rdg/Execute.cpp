//
// Aurora RDG execute phase (shared, backend-agnostic pass emission).
// Executor reads ONLY the CompiledGraph (flat, live-only, topo-ordered);
// the setup graph is never touched here.
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
        auto &cg = *mCompiledGraph;

        RDGContext ctx;
        ctx.SetCommandBuffer(cmdBuf);
        ctx.SetImageTable(&cg.resolvedImages);
        ctx.SetBufferTable(&cg.resolvedBuffers);

        for (const auto &cpass : cg.passes) {
            ctx.SetPassName(cpass.name);

            // per-pass barriers: contiguous segment in the flat barrier array
            for (uint32_t i = 0; i < cpass.barrierCount; ++i) {
                cmdBuf->PipelineBarrier(cg.barriers[cpass.barrierOffset + i]);
            }

            switch (cpass.type) {
            case CompiledPassType::SCENE_RASTER: {
                const auto &p = std::get<SceneRasterPayload>(cpass.payload);

                RenderingInfo info{};
                info.renderArea = {{0, 0}, p.renderArea};
                info.numColors = static_cast<uint32_t>(p.colors.size());
                for (size_t i = 0; i < p.colors.size() && i < MAX_COLOR_ATTACHMENTS; ++i) {
                    info.colors[i].image      = p.colors[i].image;
                    info.colors[i].loadOp     = p.colors[i].loadOp;
                    info.colors[i].storeOp    = p.colors[i].storeOp;
                    info.colors[i].clearValue = p.colors[i].clearValue;
                }
                if (p.depthStencil.image != nullptr) {
                    info.depthStencil.image          = p.depthStencil.image;
                    info.depthStencil.depthLoadOp    = p.depthStencil.depthLoadOp;
                    info.depthStencil.depthStoreOp   = p.depthStencil.depthStoreOp;
                    info.depthStencil.stencilLoadOp  = p.depthStencil.stencilLoadOp;
                    info.depthStencil.stencilStoreOp = p.depthStencil.stencilStoreOp;
                    info.depthStencil.clearValue     = p.depthStencil.clearValue;
                }

                auto enc = cmdBuf->CreateGraphicsEncoder();
                enc->BeginRendering(info);
                if (cg.globalResourceGroup != nullptr) {
                    enc->BindResourceGroup(0, cg.globalResourceGroup, 0, nullptr);
                }
                for (const auto &queue : p.queues) {
                    ResourceGroup *set1 = queue.queueResourceGroup != nullptr ? queue.queueResourceGroup : p.passResourceGroup;
                    if (set1 != nullptr) {
                        enc->BindResourceGroup(1, set1, 0, nullptr);
                    }
                    for (const auto &item : queue.items) {
                        if (item.batchResourceGroup != nullptr) {
                            enc->BindResourceGroup(2, item.batchResourceGroup, 1, &item.batchDynamicOffset);
                        }
                        if (item.pso != nullptr) {
                            enc->BindPipeline(item.pso);
                        }
                        if (item.ib != nullptr) {
                            enc->BindIndexBuffer(item.ib, item.ibOffset, IndexType::U32);
                            enc->DrawIndexed(item.args);
                        }
                    }
                }
                enc->EndRendering();
                break;
            }
            case CompiledPassType::FULLSCREEN: {
                const auto &p = std::get<FullScreenPayload>(cpass.payload);

                RenderingInfo info{};
                info.renderArea = {{0, 0}, p.renderArea};
                info.numColors = static_cast<uint32_t>(p.colors.size());
                for (size_t i = 0; i < p.colors.size() && i < MAX_COLOR_ATTACHMENTS; ++i) {
                    info.colors[i].image      = p.colors[i].image;
                    info.colors[i].loadOp     = p.colors[i].loadOp;
                    info.colors[i].storeOp    = p.colors[i].storeOp;
                    info.colors[i].clearValue = p.colors[i].clearValue;
                }
                if (p.depthStencil.image != nullptr) {
                    info.depthStencil.image          = p.depthStencil.image;
                    info.depthStencil.depthLoadOp    = p.depthStencil.depthLoadOp;
                    info.depthStencil.depthStoreOp   = p.depthStencil.depthStoreOp;
                    info.depthStencil.stencilLoadOp  = p.depthStencil.stencilLoadOp;
                    info.depthStencil.stencilStoreOp = p.depthStencil.stencilStoreOp;
                    info.depthStencil.clearValue     = p.depthStencil.clearValue;
                }

                auto enc = cmdBuf->CreateGraphicsEncoder();
                enc->BeginRendering(info);
                if (cg.globalResourceGroup != nullptr) {
                    enc->BindResourceGroup(0, cg.globalResourceGroup, 0, nullptr);
                }
                if (p.passResourceGroup != nullptr) {
                    enc->BindResourceGroup(1, p.passResourceGroup, 0, nullptr);
                }
                if (p.pso != nullptr) {
                    enc->BindPipeline(p.pso);
                    CmdDrawLinear draw{};
                    draw.vertexCount   = 3;
                    draw.firstVertex   = 0;
                    draw.instanceCount = 1;
                    draw.firstInstance = 0;
                    enc->Draw(draw);
                }
                enc->EndRendering();
                break;
            }
            case CompiledPassType::COMPUTE: {
                const auto &p = std::get<ComputePayload>(cpass.payload);
                auto enc = cmdBuf->CreateComputeEncoder();
                if (cg.globalResourceGroup != nullptr) {
                    enc->BindResourceGroup(0, cg.globalResourceGroup, 0, nullptr);
                }
                if (p.pso != nullptr) {
                    enc->BindPipeline(p.pso);
                }
                if (p.passResourceGroup != nullptr) {
                    enc->BindResourceGroup(1, p.passResourceGroup, 0, nullptr);
                }
                if (p.executeFn) {
                    p.executeFn(*enc, ctx);
                }
                break;
            }
            case CompiledPassType::COPYBLIT: {
                const auto &p = std::get<CopyBlitPayload>(cpass.payload);
                auto enc = cmdBuf->CreateBlitEncoder();
                if (p.executeFn) {
                    p.executeFn(*enc, ctx);
                }
                break;
            }
            case CompiledPassType::PRESENT:
                // no extra encoder ops; barrier already emitted in per-pass segment
                break;
            case CompiledPassType::CUSTOM: {
                const auto &p = std::get<CustomPayload>(cpass.payload);
                if (p.fn) {
                    p.fn(ctx, *cmdBuf);
                }
                break;
            }
            }
        }

        // frame-end barrier segment
        for (uint32_t i = cg.finalBarrierOffset; i < cg.barriers.size(); ++i) {
            cmdBuf->PipelineBarrier(cg.barriers[i]);
        }
    }

} // namespace sky::aurora
