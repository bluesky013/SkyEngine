//
// Created by Zach Lee on 2023/8/27.
//

#include <render/rdg/RenderSceneVisitor.h>
#include <render/RenderScene.h>
#include <render/RenderPrimitive.h>

namespace sky::rdg {

    void RenderSceneVisitor::BuildRenderQueue()
    {
        const auto &primitives = graph.scene->GetPrimitives();
        for (auto &queue : graph.rasterQueues) {
            const auto &subPass    = graph.subPasses[Index(queue.passID, graph)];
            const auto &rasterPass = graph.rasterPasses[Index(subPass.parent, graph)];
            const auto &renderPass = rasterPass.renderPass;

            for (const auto &prim : primitives) {
                if (queue.culling && !queue.sceneView->FrustumCulling(prim->boundingBox)) {
                    continue;
                }

                for (auto &tech : prim->techniques) {
                    if ((tech.viewMask & queue.viewMask) != tech.viewMask || tech.rasterID != queue.rasterID) {
                        continue;
                    }

                    if (!tech.pso || (tech.renderPass->GetCompatibleHash() != renderPass->GetCompatibleHash())) {
                        tech.renderPass = renderPass;
                        tech.pso = GraphicsTechnique::BuildPso(*tech.technique, tech.vertexDesc, renderPass, subPass.subPassID, tech.programKey);
                    }

                    queue.drawItems.emplace_back(RenderDrawItem{prim, 0});
                }
            }
        }
    }


} // namespace sky::rdg
