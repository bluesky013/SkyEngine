//
// Created by Zach Lee on 2023/4/2.
//

#include <render/RenderScene.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    RenderScene::RenderScene() : sceneViews(&resources), primitives(&resources)
    {
    }

    void RenderScene::PreTick(float time)
    {

    }

    void RenderScene::PostTick(float time)
    {

    }

    void RenderScene::Render()
    {
        rdg::RenderGraph rdg(pipeline->Context());
        pipeline->OnSetup(rdg);
        pipeline->Execute(rdg);
    }

    void RenderScene::SetPipeline(RenderPipeline *ppl)
    {
        pipeline.reset(ppl);
    }

    void RenderScene::AddSceneView(SceneView *view)
    {
        sceneViews.emplace_back(view);
    }

    void RenderScene::RemoveSceneView(SceneView *view)
    {
        sceneViews.erase(std::remove_if(sceneViews.begin(), sceneViews.end(), [&view](const auto &v) {
            return view == v;
        }), sceneViews.end());
    }

    void RenderScene::AddPrimitive(RenderPrimitive *primitive)
    {
        primitives.emplace_back(primitive);
    }

    void RenderScene::RemovePrimitive(RenderPrimitive *primitive)
    {
        primitives.erase(std::remove_if(primitives.begin(), primitives.end(), [primitive](const auto &v) {
            return primitive == v;
        }), primitives.end());
    }

} // namespace sky
