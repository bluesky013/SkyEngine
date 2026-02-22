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

        for (auto &[name, view] : viewMap) {
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
        sceneViews.emplace_back(new SceneView(viewCount, &resources));
        return sceneViews.back().get();
    }

    void RenderScene::DetachSceneView(SceneView* sceneView, const Name &name)
    {
        auto iter = viewMap.find(name);
        if (iter != viewMap.end() && iter->second == sceneView) {
            viewMap.erase(iter);
        }
    }

    void RenderScene::AttachSceneView(SceneView* sceneView, const Name &name)
    {
        SKY_ASSERT(sceneView != nullptr);
        viewMap[name] = sceneView;
    }

    SceneView *RenderScene::GetSceneView(const Name& name) const
    {
        auto iter = viewMap.find(name);
        return iter != viewMap.end() ? iter->second : nullptr;
    }

    void RenderScene::RemoveSceneView(SceneView *view)
    {
        sceneViews.erase(std::remove_if(sceneViews.begin(), sceneViews.end(), [&view](const auto &v) {
            return view == v.get();
        }), sceneViews.end());
    }

    void RenderScene::AddPrimitive(RenderPrimitive *primitive)
    {
        if (primitive == nullptr) {
            return;
        }

        auto iter = std::find(primitives.begin(), primitives.end(), primitive);
        SKY_ASSERT(iter == primitives.end());
        primitives.emplace_back(primitive);
    }

    void RenderScene::RemovePrimitive(RenderPrimitive *primitive)
    {
        if (primitive == nullptr) {
            return;
        }

        primitives.erase(std::remove_if(primitives.begin(), primitives.end(), [primitive](const auto &v) {
            return primitive == v;
        }), primitives.end());
    }

    void RenderScene::AddFeature(IFeatureProcessor *feature)
    {
        features.emplace(feature->GetTypeID(), feature);
    }

} // namespace sky
