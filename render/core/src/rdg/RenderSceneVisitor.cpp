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
        const auto &primitives = graph.scene->GetPrimitives();
        for (auto &queue : graph.rasterQueues) {
            const auto &subPass    = graph.subPasses[Index(queue.passID, graph)];
            const auto &rasterPass = graph.rasterPasses[Index(subPass.parent, graph)];
            const auto &renderPass = rasterPass.renderPass;

            for (const auto &prim : primitives) {
                if (queue.culling && queue.sceneView != nullptr && !queue.sceneView->FrustumCulling(prim->boundingBox)) {
                    continue;
                }

                for (auto &tech : prim->techniques) {
                    uint32_t viewMask = tech.technique->GetViewMask();
                    uint32_t rasterID = tech.technique->GetRasterID();

                    uint32_t sceneMask = queue.sceneView != nullptr ? queue.sceneView->GetViewMask() : 0xFFFFFFFF;
                    if ((sceneMask & viewMask) != sceneMask || rasterID != queue.rasterID) {
                        continue;
                    }

                    if (!tech.pso || (tech.renderPass->GetCompatibleHash() != renderPass->GetCompatibleHash())) {
                        tech.renderPass = renderPass;
                        tech.pso = GraphicsTechnique::BuildPso(*tech.technique, renderPass, subPass.subPassID, tech.programKey);
                    }

                    queue.drawItems.emplace_back(RenderDrawItem{prim, 0});
                }
            }
        }
    }


} // namespace sky::rdg
