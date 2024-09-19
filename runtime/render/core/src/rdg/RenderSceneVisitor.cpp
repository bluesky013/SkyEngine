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
            const auto &subPass = graph.subPasses[Index(queue.passID, graph)];
            const auto &rasterPass = graph.rasterPasses[Index(subPass.parent, graph)];
            const auto &renderPass = rasterPass.renderPass;

            auto passHash = renderPass->GetCompatibleHash();

            for (const auto &prim : primitives) {
                if (!prim->isReady) {
                    continue;
                }

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

                    bool needRebuildPso = false;

                    uint32_t optionHash = 0;
                    if (tech.shaderOption) {
                        if (tech.material) {
                            tech.material->ProcessShaderOption(tech.shaderOption);
                        }
                        optionHash = tech.shaderOption->GetHash();
                    }
                    if (!tech.program || tech.variantHash != optionHash) {
                        tech.variantHash = optionHash;
                        tech.program = tech.technique->RequestProgram(tech.shaderOption);
                        needRebuildPso = true;
                    }

                    if (prim->geometry && prim->geometry->version > tech.vaoVersion) {
                        tech.vaoVersion = prim->geometry->version;
                        tech.vao = prim->geometry->Request(tech.program, tech.vertexDesc);
                    }

                    if (tech.renderPassHash != passHash) {
                        tech.renderPassHash = passHash;

                        needRebuildPso = true;
                    }

                    if (needRebuildPso) {
                        tech.pso = GraphicsTechnique::BuildPso(tech.program,
                                                               tech.technique->GetPipelineState(),
                                                               tech.vertexDesc,
                                                               renderPass,
                                                               subPass.subPassID);
                    }

                    queue.drawItems.emplace_back(RenderDrawItem{prim, techIndex++});
                }
            }
        }
    }


} // namespace sky::rdg
