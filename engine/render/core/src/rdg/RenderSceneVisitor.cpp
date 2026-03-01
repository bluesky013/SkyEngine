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
    struct RenderItemGatherContext : IRenderItemGatherContext {
        RenderItemGatherContext(RenderGraph &g, RasterQueue& queue, SceneView* view)
            : context(g.context)
            , rasterQueue(queue)
        {
            rasterID = queue.rasterID;
            sceneView = view;
        }

        void Append(RenderItem&& item) override
        {
            rasterQueue.renderItems.emplace_back(std::move(item));
        }

        RenderGraphContext *context;
        RasterQueue& rasterQueue;
    };

    RenderSceneVisitor::RenderSceneVisitor(RenderGraph &g, RenderScene *scn)
        : graph(g)
        , scene(scn)
        , primitives(scene->GetPrimitives())
        , visibleInfos(&g.context->resources)
    {
        visibleInfos.resize(primitives.size());
    }

    void RenderSceneVisitor::Execute()
    {
        PerformCulling();
        DispatchToRenderQueue();
    }

    void RenderSceneVisitor::DispatchToRenderQueue()
    {
        for (auto& queue : graph.rasterQueues) {
            const auto &subPass = graph.subPasses[Index(queue.passID, graph)];
            const auto &rasterPass = graph.rasterPasses[Index(subPass.parent, graph)];


            RenderBatchPrepareInfo batchInfo = {
                .techId = queue.rasterID,
                .pipelineKey = rasterPass.pipelineKey,
                .pass = rasterPass.renderPass,
                .subPassId = subPass.subPassID
            };

            RenderItemGatherContext gatherContext(graph, queue, nullptr);

            uint8_t viewId = INVALID_VIEW_INDEX;

            if (queue.viewID != INVALID_VERTEX) {
                viewId = static_cast<uint8_t>(Index(queue.viewID, graph));
                gatherContext.sceneView = graph.sceneViews[viewId].sceneView;
            }

            for (uint32_t i = 0; i < visibleInfos.size(); ++i) {
                if (!visibleInfos[i].IsActive()) {
                    continue;
                }

                if (viewId != INVALID_VIEW_INDEX && !visibleInfos[i].IsActiveInView(viewId)) {
                    continue;
                }

                if (primitives[i]->PrepareBatch(batchInfo)) {
                    primitives[i]->GatherRenderItem(&gatherContext);
                }
            }
        }
    }

    void RenderSceneVisitor::PerformCulling()
    {
        const auto& activeSceneViews = scene->GetActiveSceneViews();
        const auto& cullingSystems = scene->GetCullingSystem();

        std::unordered_map<Name, std::unordered_map<SceneView*, CounterPtr<RenderSceneCullingViewData>>> viewCullingData;

        for (auto& [name, system] : cullingSystems) {
            auto& viewDataMap = viewCullingData[name];
            if (!system->IsActive()) {
                continue;
            }

            for (auto& [viewName, sceneView] : activeSceneViews) {
                auto *data = system->PrepareCullingViewData(sceneView);
                if (data != nullptr) {
                    viewDataMap[sceneView] = data;
                }
            }
        }

        for (uint32_t primIndex = 0; primIndex < primitives.size(); ++primIndex) {
            auto* prim = primitives[primIndex];
            auto& visibleInfo = visibleInfos[primIndex];

            for (uint32_t viewId = 0; viewId < graph.sceneViews.size(); ++viewId) {
                const auto &rdgView = graph.sceneViews[viewId];
                bool visibleInView = true;

                for (auto& [name, cullingSys] : cullingSystems) {
                    const auto &data = viewCullingData[name][rdgView.sceneView];
                    if (!data) {
                        continue;
                    }

                    if (prim->uniqueID != ~(0U) && !cullingSys->QueryVisible(data.Get(), prim->uniqueID)) {
                        visibleInView = false;
                        break;
                    }
                }

                if (!visibleInView) {
                    continue;
                }

                if (prim->shouldUseFrustumCulling && !rdgView.sceneView->FrustumCulling(prim->worldBounds)) {
                    continue;
                }

                visibleInfo.SetActiveInView(viewId);
                prim->Prepare(rdgView.sceneView);
            }

            if (prim->IsReady()) {
                visibleInfo.SetActive();
            }
        }
    }
} // namespace sky::rdg
