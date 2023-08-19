//
// Created by Zach Lee on 2023/4/2.
//

#include <render/RenderScene.h>

namespace sky {

    RenderScene::RenderScene() : sceneViews(&resources)
    {
    }

    void RenderScene::PreTick(float time)
    {

    }

    void RenderScene::PostTick(float time)
    {

    }

    void RenderScene::AddSceneView(const ViewPtr &view)
    {
        sceneViews.emplace_back(view);
    }

    void RenderScene::RemoveSceneView(const ViewPtr &view)
    {
        sceneViews.erase(std::remove_if(sceneViews.begin(), sceneViews.end(), [&view](const auto &v) {
            return view.get() == v.get();
        }), sceneViews.end());
    }
} // namespace sky