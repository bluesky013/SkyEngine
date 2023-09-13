//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <RDSceneProject/ProjectRoot.h>

#include <framework/serialization/CoreReflection.h>
#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindow.h>

#include <render/Renderer.h>

#include "scenes/SampleSceneCube.h"

namespace sky {

    bool RDSceneProject::Init()
    {
        AssetManager::Get()->RegisterSearchPath(ENGINE_ROOT + "/assets");
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT + "/assets");
        AssetManager::Get()->RegisterSearchPath(PROJECT_ROOT + "/cache");
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

        Event<IWindowEvent>::Connect(nativeWindow, this);

        sampleScenes.emplace("Cube", new SampleSceneCube());
        currentScene = sampleScenes.begin()->second.get();
        currentScene->Start(window);
    }

    void RDSceneProject::Stop()
    {
        if (currentScene != nullptr) {
            currentScene->Shutdown();
        }
        sampleScenes.clear();

        auto *renderer = Renderer::Get();
        renderer->DestroyRenderWindow(window);

        Event<IWindowEvent>::DisConnect(this);
    }

    void RDSceneProject::Tick(float delta)
    {
        if (currentScene != nullptr) {
            currentScene->Tick(delta);
        }
    }

} // namespace sky

REGISTER_MODULE(sky::RDSceneProject)
