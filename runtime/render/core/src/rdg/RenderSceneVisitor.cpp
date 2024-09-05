//
// Created by Zach Lee on 2023/8/27.
//

#include <render/rdg/RenderSceneVisitor.h>
#include <render/RenderScene.h>
#include <render/RenderPrimitive.h>
#include <render/rdg/RenderGraph.h>

namespace sky::rdg {

    void RenderSceneVisitor::BuildRenderQueue()
    {
        const auto &primitives = scene->GetPrimitives();
        for (auto &queue : graph.rasterQueues) {
            const auto &subPass    = graph.subPasses[Index(queue.passID, graph)];
            const auto &rasterPass = graph.rasterPasses[Index(subPass.parent, graph)];
            const auto &renderPass = rasterPass.renderPass;

            for (const auto &prim : primitives) {
                if (queue.culling && queue.sceneView != nullptr && !queue.sceneView->FrustumCulling(prim->worldBound)) {
                    continue;
                }

                uint32_t techIndex = 0;
                for (auto &tech : prim->techniques) {
                    uint32_t viewMask = tech.technique->GetViewMask();
                    uint32_t rasterID = tech.technique->GetRasterID();

                    uint32_t sceneMask = queue.sceneView != nullptr ? queue.sceneView->GetViewMask() : 0xFFFFFFFF;
                    if ((sceneMask & viewMask) != sceneMask || rasterID != queue.rasterID) {
                        techIndex++;
                        continue;
                    }

                    if (tech.rebuildPso || !tech.pso || (tech.renderPass->GetCompatibleHash() != renderPass->GetCompatibleHash())) {
                        tech.renderPass = renderPass;
                        tech.pso = GraphicsTechnique::BuildPso(*tech.technique, renderPass, subPass.subPassID);

                        tech.rebuildPso = false;
                    }

                    queue.drawItems.emplace_back(RenderDrawItem{prim, techIndex++});
                }
            }
        }
    }


} // namespace sky::rdg
