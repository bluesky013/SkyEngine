//
// Created by Zach Lee on 2023/8/27.
//

#include <render/rdg/RenderSceneVisitor.h>
#include <render/RenderPrimitive.h>
#include <render/rdg/RenderGraph.h>
#include <render/RenderScene.h>
#include <core/logger/Logger.h>

static const char *TAG = "Renderer";

namespace sky::rdg {
    static void BuildRenderBatch(RenderPrimitive* primitive, uint32_t batchIndex, const RasterPass &pass, uint32_t subPassId)
    {
        auto &batch = primitive->batches[batchIndex];

        ShaderVariantKey vertexKey = {};
        batch.technique->ProcessVertexVariantKey(primitive->vertexFlags, vertexKey);

        ShaderVariantKey final = pass.passKey | vertexKey | batch.batchKey;

        bool needRebuildPso = false;
        if (final != batch.cacheFinalKey || !batch.program) {
            batch.cacheFinalKey = final;
            batch.program = batch.technique->RequestProgram(final, primitive->clusterValid);

            if (batch.program) {
                batch.vertexDesc = primitive->geometry->Request(batch.program);
            } else {
                LOG_E(TAG, "requst program failed %s", final.ToString().c_str());
            }

            needRebuildPso = true;
        }

        uint32_t passHash = pass.renderPass->GetCompatibleHash();
        if (batch.renderPassHash != passHash) {
            batch.renderPassHash = passHash;
            needRebuildPso = true;
        }

        if (needRebuildPso) {
            needRebuildPso &= static_cast<bool>(batch.program);
//            needRebuildPso &= static_cast<bool>(batch.vertexDesc);
            needRebuildPso &= static_cast<bool>(pass.renderPass);

            if (needRebuildPso) {
                auto pState = batch.technique->GetPipelineState();

                // process override states.
                pState.inputAssembly.topology = batch.topo;

                batch.pso = GraphicsTechnique::BuildPso(batch.program, pState, batch.vertexDesc, pass.renderPass, subPassId);
            }
        }

        // build vertex && index
        if (primitive->geometry && primitive->geometry->version != batch.vaoVersion) {
            if (!primitive->geometry->dynamicVB) {
                batch.vao = primitive->geometry->Request(batch.program, batch.vertexDesc);
            }
            batch.vaoVersion = primitive->geometry->version;
        }
    }

    static void BuildRenderPrimitive(RenderGraph &graph, RasterQueue &queue, RenderPrimitive* primitive)
    {
        const auto &subPass = graph.subPasses[Index(queue.passID, graph)];
        const auto &rasterPass = graph.rasterPasses[Index(subPass.parent, graph)];

        for (uint32_t i = 0; i < primitive->batches.size(); ++i) {
            auto &batch = primitive->batches[i];

            uint32_t viewMask = batch.technique->GetViewMask();
            Name rasterID = batch.technique->GetRasterID();

            uint32_t sceneMask = queue.sceneView != nullptr ? queue.sceneView->GetViewMask() : 0xFFFFFFFF;
            if ((sceneMask & viewMask) != sceneMask || rasterID != queue.rasterID) {
                continue;
            }
            BuildRenderBatch(primitive, i, rasterPass, subPass.subPassID);

            if (batch.pso) {
                queue.drawItems.emplace_back(RenderDrawItem{
                    primitive, i
                });
            }
        }
    }

    void RenderSceneVisitor::BuildRenderQueue()
    {
        const auto &primitives = scene->GetPrimitives();

        for (const auto &prim : primitives) {
            prim->PrepareBatch();
        }

        for (auto &queue : graph.rasterQueues) {
            for (const auto &prim : primitives) {
                if (!prim->geometry || !prim->IsReady()) {
                    continue;
                }

                if (queue.culling && queue.sceneView != nullptr && !queue.sceneView->FrustumCulling(prim->worldBound)) {
                    continue;
                }
                BuildRenderPrimitive(graph, queue, prim);
            }
        }

        for (const auto &prim : primitives) {
            prim->UpdateBatch();
        }
    }
} // namespace sky::rdg
