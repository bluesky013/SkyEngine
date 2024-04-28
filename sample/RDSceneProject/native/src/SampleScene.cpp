//
// Created by Zach Lee on 2023/9/2.
//

#include <SampleScene.h>
#include <render/Renderer.h>
#include <render/adaptor/pipeline/DefaultForward.h>

namespace sky {

    bool SampleScene::Start(RenderWindow *window)
    {
        world = std::make_unique<World>();

        sceneProxy = std::make_unique<RenderSceneProxy>();
        auto *renderScene = sceneProxy->GetRenderScene();
        auto *ppl = new DefaultForward();
        ppl->SetOutput(window);
        Renderer::Get()->SetPipeline(ppl);

        world->AddSubSystem("RenderScene", sceneProxy.get());
        return true;
    }

    void SampleScene::Shutdown()
    {
        world = nullptr;
        sceneProxy = nullptr;
    }

    void SampleScene::Tick(float time)
    {
        world->Tick(time);
    }

} // namespace sky
