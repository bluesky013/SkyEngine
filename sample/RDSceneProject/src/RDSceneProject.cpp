//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <RDSceneProject/ProjectRoot.h>

#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindow.h>

#include <render/Renderer.h>
#include <render/pipeline/DefaultForward.h>
#include <render/adaptor/assets/TechniqueAsset.h>

namespace sky {

    bool RDSceneProject::Init()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT + "/assets");
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT + "/assets");
        AssetManager::Get()->Reset(PROJECT_ROOT + "/assets.db");

        return true;
    }

    void RDSceneProject::OnWindowResize(uint32_t width, uint32_t height)
    {
        if (window != nullptr) {
            window->Resize(width, height);
        }
    }

    void RDSceneProject::Start()
    {
        auto *renderer = Renderer::Get();
        const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        window = renderer->CreateRenderWindow(nativeWindow->GetNativeHandle(), nativeWindow->GetWidth(), nativeWindow->GetHeight(), false);

        scene = renderer->CreateScene();
        auto *ppl = new DefaultForward();
        ppl->SetOutput(window);
        scene->SetPipeline(ppl);

        Event<IWindowEvent>::Connect(nativeWindow, this);

        auto asset = AssetManager::Get()->LoadAsset<Technique>("techniques/gui.tech", "GFX_TECH");
    }

    void RDSceneProject::Stop()
    {
        auto *renderer = Renderer::Get();
        renderer->RemoveScene(scene);
        renderer->DestroyRenderWindow(window);

        Event<IWindowEvent>::DisConnect(this);
    }

    void RDSceneProject::Tick(float delta)
    {
    }

} // namespace sky

REGISTER_MODULE(sky::RDSceneProject)