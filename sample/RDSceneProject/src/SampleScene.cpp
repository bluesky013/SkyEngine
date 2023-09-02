//
// Created by Zach Lee on 2023/9/2.
//

#include <SampleScene.h>
#include <render/Renderer.h>
#include <render/pipeline/DefaultForward.h>

namespace sky {

    bool SampleScene::Start(RenderWindow *window)
    {
        auto *renderer = Renderer::Get();
        scene = renderer->CreateScene();
        auto *ppl = new DefaultForward();
        ppl->SetOutput(window);
        scene->SetPipeline(ppl);

        return true;
    }

    void SampleScene::Shutdown()
    {
        auto *renderer = Renderer::Get();
        renderer->RemoveScene(scene);
    }

} // namespace sky