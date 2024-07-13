//
// Created by Zach Lee on 2023/4/2.
//

#include <render/RenderScene.h>
#include <render/rdg/RenderGraph.h>

namespace sky {

    RenderScene::RenderScene()
        : features(&resources)
        , sceneViews(&resources)
        , primitives(&resources)
    {
    }

    RenderScene::~RenderScene()
    {
        features.clear();
    }

    void RenderScene::PreTick(float time)
    {
        for (auto &feature : features) {
            feature.second->Tick(time);
        }

        for (auto &view : sceneViews) {
            view->Update();
        }
    }

    void RenderScene::PostTick(float time)
    {

    }

    void RenderScene::Render(rdg::RenderGraph& rdg)
    {
        for (auto &feature : features) {
            feature.second->Render(rdg);
        }
    }

    SceneView *RenderScene::CreateSceneView(uint32_t viewCount)
    {
        sceneViews.emplace_back(new SceneView(viewCounter++, viewCount, &resources));
        return sceneViews.back().get();
    }

    void RenderScene::RemoveSceneView(SceneView *view)
    {
        sceneViews.erase(std::remove_if(sceneViews.begin(), sceneViews.end(), [&view](const auto &v) {
            return view == v.get();
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

    void RenderScene::AddFeature(IFeatureProcessor *feature)
    {
        features.emplace(feature->GetTypeID(), feature);
    }

} // namespace sky
