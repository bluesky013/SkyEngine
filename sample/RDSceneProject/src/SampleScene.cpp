//
// Created by Zach Lee on 2023/9/2.
//

#include <SampleScene.h>
#include <render/Renderer.h>
#include <render/pipeline/DefaultForward.h>

namespace sky {

    bool SampleScene::Start(RenderWindow *window)
    {
        world = std::make_unique<World>();

        sceneProxy = std::make_unique<RenderSceneProxy>();
        auto *renderScene = sceneProxy->GetRenderScene();
        auto *ppl = new DefaultForward();
        ppl->SetOutput(window);
        renderScene->SetPipeline(ppl);

        world->SetRenderScene(sceneProxy.get());
        return true;
    }

    void SampleScene::Shutdown()
    {
        sceneProxy = nullptr;
        world = nullptr;
    }

    void SampleScene::Tick(float time)
    {
        world->Tick(time);
    }

} // namespace sky
