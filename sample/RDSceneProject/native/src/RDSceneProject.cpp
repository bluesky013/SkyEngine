//
// Created by Zach Lee on 2022/8/13.
//

#include <RDSceneProject.h>
#include <RDSceneProject/ProjectRoot.h>

#include <cxxopts.hpp>

#include <framework/asset/AssetManager.h>
#include <framework/window/NativeWindow.h>

#include <render/Renderer.h>
#include <render/RHI.h>

#include "scenes/SampleSceneCube.h"
#include "scenes/SampleMesh.h"

namespace sky {

    bool RDSceneProject::Init(const StartArguments &args)
    {
        cxxopts::Options options("SkyEngine Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();
        options.add_options()("a, app", "app mode", cxxopts::value<std::string>());
        auto result = options.parse(static_cast<int>(args.args.size()), args.args.data());
        bool isXRMode = (result.count("app") != 0u) && result["app"].as<std::string>() == "xr";

        auto *renderer = Renderer::Get();

        if (!isXRMode) {
            const auto *nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
            window = renderer->CreateRenderWindow(nativeWindow->GetNativeHandle(), nativeWindow->GetWidth(), nativeWindow->GetHeight(), false);
            Event<IWindowEvent>::Connect(nativeWindow, this);
        }
#ifdef SKY_ENABLE_XR
        else {
            window = renderer->CreateRenderWindowByXR();
        }
#endif

        return window != nullptr;
    }

    void RDSceneProject::Start()
    {
        sampleScenes.emplace_back(new SampleMesh());
//        sampleScenes.emplace_back(new SampleSceneCube());
        sceneIndex = static_cast<uint32_t>(sampleScenes.size()) - 1;
        NextScene();
    }

    void RDSceneProject::OnWindowResize(uint32_t width, uint32_t height)
    {
        if (window != nullptr) {
            window->Resize(width, height);
        }
        if (currentScene != nullptr) {
            currentScene->Resize(width, height);
        }
    }

    void RDSceneProject::OnKeyUp(KeyButtonType type)
    {
        if (type == KeyButton::KEY_RIGHT) {
            NextScene();
        }
    }

    void RDSceneProject::NextScene()
    {
        if (currentScene != nullptr) {
            currentScene->Shutdown();
        }
        sceneIndex = (sceneIndex + 1) % sampleScenes.size();

        currentScene = sampleScenes[sceneIndex].get();
        currentScene->Start(window);
    }

    void RDSceneProject::Shutdown()
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
