//
// Created by Zach Lee on 2023/8/27.
//

#include <render/rdg/RenderSceneVisitor.h>
#include <render/RenderScene.h>
#include <render/RenderPrimitive.h>

namespace sky::rdg {

    void RenderSceneVisitor::discover_vertex(Vertex u, const Graph& g)
    {
        const auto &primitives = graph.scene->GetPrimitives();
        for (const auto &prim : primitives) {
            for (auto &tech : prim->techniques) {
                for (auto &queue : graph.rasterQueues) {
                    if ((tech.viewMask & queue.viewMask) != tech.viewMask ||
                        tech.rasterID != queue.rasterID) {
                        continue;
                    }

                    queue.drawItems.emplace_back(RenderDrawItem{prim, 0});
                }
            }
        }
    }


} // namespace sky::rdg